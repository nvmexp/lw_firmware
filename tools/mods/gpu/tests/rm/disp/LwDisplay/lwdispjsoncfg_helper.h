#include "core/include/color.h"
#include "gpu/utility/surf2d.h"
#include "class/clc37d.h"
#include "ctrl/ctrl0073.h"

typedef struct _ClrUtilString
{
    ColorUtils::Format  clrFmt;
    std::string         fmtStr;
} ClrUtilString;

typedef struct _Surface2DLayoutStr
{
    Surface2D::Layout   layout;
    std::string         layoutStr;
} Surface2DLayoutStr;

typedef struct _Surface2DInputRangeStr
{
    Surface2D::InputRange inputRange;
    std::string         inputRangeStr;
} Surface2DInputRangeStr;

typedef struct _OrType
{
    LwDispORSettings::ORType orType;
    std::string              orTypeStr;
} OrType;

typedef struct _OrProtocol
{
    LwU32        orProtocol;
    std::string orProtocolStr;
} OrProtocol;

typedef struct _OrPixelDepth
{
    LwDispORSettings::PixelDepth    pixelDepth;
    std::string                         pixelDepthStr;
} OrPixelDepth;

OrPixelDepth orPixelDepthStr[] =
{
    { LwDispORSettings::PixelDepth::OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_16_422, "BPP_16_422"},
    { LwDispORSettings::PixelDepth::OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_18_444, "BPP_18_444"},
    { LwDispORSettings::PixelDepth::OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_20_422, "BPP_20_422"},
    { LwDispORSettings::PixelDepth::OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_24_422, "BPP_24_422"},
    { LwDispORSettings::PixelDepth::OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_24_444, "BPP_24_444"},
    { LwDispORSettings::PixelDepth::OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_30_444, "BPP_30_444"},
    { LwDispORSettings::PixelDepth::OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_32_422, "BPP_16_422"},
    { LwDispORSettings::PixelDepth::OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_36_444, "BPP_36_444"},
    { LwDispORSettings::PixelDepth::OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_48_444, "BPP_48_444"},
};

OrProtocol orProtocolStr[] = {
    {LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DP_A, "DP_A"},
    {LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DP_B, "DP_B"},
    {LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_SINGLE_TMDS_A, "TMDS_A"},
    {LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_SINGLE_TMDS_A, "TMDS_B"},
    {LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DUAL_TMDS, "DUAL_TMDS"},
};

OrType orTypeStr[] = {
        {LwDispORSettings::ORType::SOR,  "SOR"},
        {LwDispORSettings::ORType::PIOR, "PIOR"},
        {LwDispORSettings::ORType::WBOR, "WBOR"},
};

Surface2DInputRangeStr Surface2dInputRangeStr[] = {
    {Surface2D::BYPASS,  "BYPASS"},
    {Surface2D::LIMITED, "LIMITED"},
    {Surface2D::FULL,    "FULL"},
};

Surface2DLayoutStr Surf2dLayoutStr[] = {
      { Surface2D::Layout::Pitch, "Pitch" },
      { Surface2D::Layout::Swizzled, "Swizzled"},
      { Surface2D::Layout::BlockLinear, "BlockLinear"},
      { Surface2D::Layout::Tiled, "Tiled" },
};

ClrUtilString ClrUtilStr[] = {
    { ColorUtils::LWFMT_NONE,"LWFMT_NONE"},
    { ColorUtils::R5G6B5,"R5G6B5"},
    { ColorUtils::A8R8G8B8,"A8R8G8B8"},
    { ColorUtils::R8G8B8A8,"R8G8B8A8"},
    { ColorUtils::A2R10G10B10,"A2R10G10B10"},
    { ColorUtils::R10G10B10A2,"R10G10B10A2"},
    { ColorUtils::CR8YB8CB8YA8,"CR8YB8CB8YA8"},
    { ColorUtils::YB8CR8YA8CB8,"YB8CR8YA8CB8"},
    { ColorUtils::Z1R5G5B5,"Z1R5G5B5"},
    { ColorUtils::Z24S8,"Z24S8"},
    { ColorUtils::Z16,"Z16"},
    { ColorUtils::RF16,"RF16"},
    { ColorUtils::RF32,"RF32"},
    { ColorUtils::RF16_GF16_BF16_AF16,"RF16_GF16_BF16_AF16"},
    { ColorUtils::RF32_GF32_BF32_AF32,"RF32_GF32_BF32_AF32"},
    { ColorUtils::Y8,"Y8"},
    { ColorUtils::B8_G8_R8,"B8_G8_R8"},
    { ColorUtils::Z24,"Z24"},
    { ColorUtils::I8,"I8"},
    { ColorUtils::VOID8,"VOID8"},
    { ColorUtils::VOID16,"VOID16"},
    { ColorUtils::A2V10Y10U10,"A2V10Y10U10"},
    { ColorUtils::A2U10Y10V10,"A2U10Y10V10"},
    { ColorUtils::VE8YO8UE8YE8,"VE8YO8UE8YE8"},
    { ColorUtils::UE8YO8VE8YE8,"UE8YO8VE8YE8"},
    { ColorUtils::YO8VE8YE8UE8,"YO8VE8YE8UE8"},
    { ColorUtils::YO8UE8YE8VE8,"YO8UE8YE8VE8"},
    { ColorUtils::YE16_UE16_YO16_VE16,"YE16_UE16_YO16_VE16"},
    { ColorUtils::YE10Z6_UE10Z6_YO10Z6_VE10Z6,"YE10Z6_UE10Z6_YO10Z6_VE10Z6"},
    { ColorUtils::UE16_YE16_VE16_YO16,"UE16_YE16_VE16_YO16"},
    { ColorUtils::UE10Z6_YE10Z6_VE10Z6_YO10Z6,"UE10Z6_YE10Z6_VE10Z6_YO10Z6"},
    { ColorUtils::VOID32,"VOID32"},
    { ColorUtils::CPST8,"CPST8"},
    { ColorUtils::CPSTY8CPSTC8,"CPSTY8CPSTC8"},
    { ColorUtils::AUDIOL16_AUDIOR16,"AUDIOL16_AUDIOR16"},
    { ColorUtils::AUDIOL32_AUDIOR32,"AUDIOL32_AUDIOR32"},
    { ColorUtils::A2B10G10R10,"A2B10G10R10"},
    { ColorUtils::A8B8G8R8,"A8B8G8R8"},
    { ColorUtils::A1R5G5B5,"A1R5G5B5"},
    { ColorUtils::Z8R8G8B8,"Z8R8G8B8"},
    { ColorUtils::Z32,"Z32"},
    { ColorUtils::X8R8G8B8,"X8R8G8B8"},
    { ColorUtils::X1R5G5B5,"X1R5G5B5"},
    { ColorUtils::AN8BN8GN8RN8,"AN8BN8GN8RN8"},
    { ColorUtils::AS8BS8GS8RS8,"AS8BS8GS8RS8"},
    { ColorUtils::AU8BU8GU8RU8,"AU8BU8GU8RU8"},
    { ColorUtils::X8B8G8R8,"X8B8G8R8"},
    { ColorUtils::A8RL8GL8BL8,"A8RL8GL8BL8"},
    { ColorUtils::X8RL8GL8BL8,"X8RL8GL8BL8"},
    { ColorUtils::A8BL8GL8RL8,"A8BL8GL8RL8"},
    { ColorUtils::X8BL8GL8RL8,"X8BL8GL8RL8"},
    { ColorUtils::RF32_GF32_BF32_X32,"RF32_GF32_BF32_X32"},
    { ColorUtils::RS32_GS32_BS32_AS32,"RS32_GS32_BS32_AS32"},
    { ColorUtils::RS32_GS32_BS32_X32,"RS32_GS32_BS32_X32"},
    { ColorUtils::RU32_GU32_BU32_AU32,"RU32_GU32_BU32_AU32"},
    { ColorUtils::RU32_GU32_BU32_X32,"RU32_GU32_BU32_X32"},
    { ColorUtils::R16_G16_B16_A16,"R16_G16_B16_A16"},
    { ColorUtils::RN16_GN16_BN16_AN16,"RN16_GN16_BN16_AN16"},
    { ColorUtils::RU16_GU16_BU16_AU16,"RU16_GU16_BU16_AU16"},
    { ColorUtils::RS16_GS16_BS16_AS16,"RS16_GS16_BS16_AS16"},
    { ColorUtils::RF16_GF16_BF16_X16,"RF16_GF16_BF16_X16"},
    { ColorUtils::RF32_GF32,"RF32_GF32"},
    { ColorUtils::RS32_GS32,"RS32_GS32"},
    { ColorUtils::RU32_GU32,"RU32_GU32"},
    { ColorUtils::RS32,"RS32"},
    { ColorUtils::RU32,"RU32"},
    { ColorUtils::AU2BU10GU10RU10,"AU2BU10GU10RU10"},
    { ColorUtils::RF16_GF16,"RF16_GF16"},
    { ColorUtils::RS16_GS16,"RS16_GS16"},
    { ColorUtils::RN16_GN16,"RN16_GN16"},
    { ColorUtils::RU16_GU16,"RU16_GU16"},
    { ColorUtils::R16_G16,"R16_G16"},
    { ColorUtils::BF10GF11RF11,"BF10GF11RF11"},
    { ColorUtils::G8R8,"G8R8"},
    { ColorUtils::GN8RN8,"GN8RN8"},
    { ColorUtils::GS8RS8,"GS8RS8"},
    { ColorUtils::GU8RU8,"GU8RU8"},
    { ColorUtils::R16,"R16"},
    { ColorUtils::RN16,"RN16"},
    { ColorUtils::RS16,"RS16"},
    { ColorUtils::RU16,"RU16"},
    { ColorUtils::R8,"R8"},
    { ColorUtils::RN8,"RN8"},
    { ColorUtils::RS8,"RS8"},
    { ColorUtils::RU8,"RU8"},
    { ColorUtils::A8,"A8"},
    { ColorUtils::ZF32,"ZF32"},
    { ColorUtils::S8Z24,"S8Z24"},
    { ColorUtils::X8Z24,"X8Z24"},
    { ColorUtils::V8Z24,"V8Z24"},
    { ColorUtils::ZF32_X24S8,"ZF32_X24S8"},
    { ColorUtils::X8Z24_X16V8S8,"X8Z24_X16V8S8"},
    { ColorUtils::ZF32_X16V8X8,"ZF32_X16V8X8"},
    { ColorUtils::ZF32_X16V8S8,"ZF32_X16V8S8"},
    { ColorUtils::S8,"S8"},
    { ColorUtils::X2BL10GL10RL10_XRBIAS,"X2BL10GL10RL10_XRBIAS"},
    { ColorUtils::R16_G16_B16_A16_LWBIAS,"R16_G16_B16_A16_LWBIAS"},
    { ColorUtils::X2BL10GL10RL10_XVYCC,"X2BL10GL10RL10_XVYCC"},
    { ColorUtils::Y8_U8__Y8_V8_N422,"Y8_U8__Y8_V8_N422"},
    { ColorUtils::U8_Y8__V8_Y8_N422,"U8_Y8__V8_Y8_N422"},
    { ColorUtils::Y8___U8V8_N444,"Y8___U8V8_N444"},
    { ColorUtils::Y8___U8V8_N422,"Y8___U8V8_N422"},
    { ColorUtils::Y8___U8V8_N422R,"Y8___U8V8_N422R"},
    { ColorUtils::Y8___V8U8_N420,"Y8___V8U8_N420"},
    { ColorUtils::Y8___U8___V8_N444,"Y8___U8___V8_N444"},
    { ColorUtils::Y8___U8___V8_N420,"Y8___U8___V8_N420"},
    { ColorUtils::Y10___U10V10_N444,"Y10___U10V10_N444"},
    { ColorUtils::Y10___U10V10_N422,"Y10___U10V10_N422"},
    { ColorUtils::Y10___U10V10_N422R,"Y10___U10V10_N422R"},
    { ColorUtils::Y10___V10U10_N420,"Y10___V10U10_N420"},
    { ColorUtils::Y10___U10___V10_N444,"Y10___U10___V10_N444"},
    { ColorUtils::Y10___U10___V10_N420,"Y10___U10___V10_N420"},
    { ColorUtils::Y12___U12V12_N444,"Y12___U12V12_N444"},
    { ColorUtils::Y12___U12V12_N422,"Y12___U12V12_N422"},
    { ColorUtils::Y12___U12V12_N422R,"Y12___U12V12_N422R"},
    { ColorUtils::Y12___V12U12_N420,"Y12___V12U12_N420"},
    { ColorUtils::Y12___U12___V12_N444,"Y12___U12___V12_N444"},
    { ColorUtils::Y12___U12___V12_N420,"Y12___U12___V12_N420"},
    { ColorUtils::A8Y8U8V8,"A8Y8U8V8"},
    { ColorUtils::I1,"I1"},
    { ColorUtils::I2,"I2"},
    { ColorUtils::I4,"I4"},
    { ColorUtils::A4R4G4B4,"A4R4G4B4"},
    { ColorUtils::R4G4B4A4,"R4G4B4A4"},
    { ColorUtils::B4G4R4A4,"B4G4R4A4"},
    { ColorUtils::R5G5B5A1,"R5G5B5A1"},
    { ColorUtils::B5G5R5A1,"B5G5R5A1"},
    { ColorUtils::A8R6x2G6x2B6x2,"A8R6x2G6x2B6x2"},
    { ColorUtils::A8B6x2G6x2R6x2,"A8B6x2G6x2R6x2"},
    { ColorUtils::X1B5G5R5,"X1B5G5R5"},
    { ColorUtils::B5G5R5X1,"B5G5R5X1"},
    { ColorUtils::R5G5B5X1,"R5G5B5X1"},
    { ColorUtils::U8,"U8"},
    { ColorUtils::V8,"V8"},
    { ColorUtils::CR8,"CR8"},
    { ColorUtils::CB8,"CB8"},
    { ColorUtils::U8V8,"U8V8"},
    { ColorUtils::V8U8,"V8U8"},
    { ColorUtils::CR8CB8,"CR8CB8"},
    { ColorUtils::CB8CR8,"CB8CR8"},
    { ColorUtils::R32_G32_B32_A32,"R32_G32_B32_A32"},
    { ColorUtils::R32_G32_B32,"R32_G32_B32"},
    { ColorUtils::R32_G32,"R32_G32"},
    { ColorUtils::R32_B24G8,"R32_B24G8"},
    { ColorUtils::G8R24,"G8R24"},
    { ColorUtils::G24R8,"G24R8"},
    { ColorUtils::R32,"R32"},
    { ColorUtils::A4B4G4R4,"A4B4G4R4"},
    { ColorUtils::A5B5G5R1,"A5B5G5R1"},
    { ColorUtils::A1B5G5R5,"A1B5G5R5"},
    { ColorUtils::B5G6R5,"B5G6R5"},
    { ColorUtils::B6G5R5,"B6G5R5"},
    { ColorUtils::Y8_VIDEO,"Y8_VIDEO"},
    { ColorUtils::G4R4,"G4R4"},
    { ColorUtils::R1,"R1"},
    { ColorUtils::E5B9G9R9_SHAREDEXP,"E5B9G9R9_SHAREDEXP"},
    { ColorUtils::G8B8G8R8,"G8B8G8R8"},
    { ColorUtils::B8G8R8G8,"B8G8R8G8"},
    { ColorUtils::DXT1,"DXT1"},
    { ColorUtils::DXT23,"DXT23"},
    { ColorUtils::DXT45,"DXT45"},
    { ColorUtils::DXN1,"DXN1"},
    { ColorUtils::DXN2,"DXN2"},
    { ColorUtils::BC6H_SF16,"BC6H_SF16"},
    { ColorUtils::BC6H_UF16,"BC6H_UF16"},
    { ColorUtils::BC7U,"BC7U"},
    { ColorUtils::X4V4Z24__COV4R4V,"X4V4Z24__COV4R4V"},
    { ColorUtils::X4V4Z24__COV8R8V,"X4V4Z24__COV8R8V"},
    { ColorUtils::V8Z24__COV4R12V,"V8Z24__COV4R12V"},
    { ColorUtils::X8Z24_X20V4S8__COV4R4V,"X8Z24_X20V4S8__COV4R4V"},
    { ColorUtils::X8Z24_X20V4S8__COV8R8V,"X8Z24_X20V4S8__COV8R8V"},
    { ColorUtils::ZF32_X20V4X8__COV4R4V,"ZF32_X20V4X8__COV4R4V"},
    { ColorUtils::ZF32_X20V4X8__COV8R8V,"ZF32_X20V4X8__COV8R8V"},
    { ColorUtils::ZF32_X20V4S8__COV4R4V,"ZF32_X20V4S8__COV4R4V"},
    { ColorUtils::ZF32_X20V4S8__COV8R8V,"ZF32_X20V4S8__COV8R8V"},
    { ColorUtils::X8Z24_X16V8S8__COV4R12V,"X8Z24_X16V8S8__COV4R12V"},
    { ColorUtils::ZF32_X16V8X8__COV4R12V,"ZF32_X16V8X8__COV4R12V"},
    { ColorUtils::ZF32_X16V8S8__COV4R12V,"ZF32_X16V8S8__COV4R12V"},
    { ColorUtils::V8Z24__COV8R24V,"V8Z24__COV8R24V"},
    { ColorUtils::X8Z24_X16V8S8__COV8R24V,"X8Z24_X16V8S8__COV8R24V"},
    { ColorUtils::ZF32_X16V8X8__COV8R24V,"ZF32_X16V8X8__COV8R24V"},
    { ColorUtils::ZF32_X16V8S8__COV8R24V,"ZF32_X16V8S8__COV8R24V"},
    { ColorUtils::B8G8R8A8,"B8G8R8A8"},
    { ColorUtils::ASTC_2D_4X4,"ASTC_2D_4X4"},
    { ColorUtils::ASTC_2D_5X4,"ASTC_2D_5X4"},
    { ColorUtils::ASTC_2D_5X5,"ASTC_2D_5X5"},
    { ColorUtils::ASTC_2D_6X5,"ASTC_2D_6X5"},
    { ColorUtils::ASTC_2D_6X6,"ASTC_2D_6X6"},
    { ColorUtils::ASTC_2D_8X5,"ASTC_2D_8X5"},
    { ColorUtils::ASTC_2D_8X6,"ASTC_2D_8X6"},
    { ColorUtils::ASTC_2D_8X8,"ASTC_2D_8X8"},
    { ColorUtils::ASTC_2D_10X5,"ASTC_2D_10X5"},
    { ColorUtils::ASTC_2D_10X6,"ASTC_2D_10X6"},
    { ColorUtils::ASTC_2D_10X8,"ASTC_2D_10X8"},
    { ColorUtils::ASTC_2D_10X10,"ASTC_2D_10X10"},
    { ColorUtils::ASTC_2D_12X10,"ASTC_2D_12X10"},
    { ColorUtils::ASTC_2D_12X12,"ASTC_2D_12X12"},
    { ColorUtils::ASTC_SRGB_2D_4X4,"ASTC_SRGB_2D_4X4"},
    { ColorUtils::ASTC_SRGB_2D_5X4,"ASTC_SRGB_2D_5X4"},
    { ColorUtils::ASTC_SRGB_2D_5X5,"ASTC_SRGB_2D_5X5"},
    { ColorUtils::ASTC_SRGB_2D_6X5,"ASTC_SRGB_2D_6X5"},
    { ColorUtils::ASTC_SRGB_2D_6X6,"ASTC_SRGB_2D_6X6"},
    { ColorUtils::ASTC_SRGB_2D_8X5,"ASTC_SRGB_2D_8X5"},
    { ColorUtils::ASTC_SRGB_2D_8X6,"ASTC_SRGB_2D_8X6"},
    { ColorUtils::ASTC_SRGB_2D_8X8,"ASTC_SRGB_2D_8X8"},
    { ColorUtils::ASTC_SRGB_2D_10X5,"ASTC_SRGB_2D_10X5"},
    { ColorUtils::ASTC_SRGB_2D_10X6,"ASTC_SRGB_2D_10X6"},
    { ColorUtils::ASTC_SRGB_2D_10X8,"ASTC_SRGB_2D_10X8"},
    { ColorUtils::ASTC_SRGB_2D_10X10,"ASTC_SRGB_2D_10X10"},
    { ColorUtils::ASTC_SRGB_2D_12X10,"ASTC_SRGB_2D_12X10"},
    { ColorUtils::ASTC_SRGB_2D_12X12,"ASTC_SRGB_2D_12X12"},
    { ColorUtils::ETC2_RGB,"ETC2_RGB"},
    { ColorUtils::ETC2_RGB_PTA,"ETC2_RGB_PTA"},
    { ColorUtils::ETC2_RGBA,"ETC2_RGBA"},
    { ColorUtils::EAC,"EAC"},
    { ColorUtils::EACX2,"EACX2"},
};
