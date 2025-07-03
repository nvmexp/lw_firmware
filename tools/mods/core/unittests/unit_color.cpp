/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013,2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   unit_color.cpp
 * @brief  Unit tests for ColorUtils
 */

#include "unittest.h"
#include "core/include/color.h"
#include "random.h"
#include "core/include/xp.h"

namespace
{
    //----------------------------------------------------------------
    //! \brief Unit tests for the ColorUtils functions
    //!
    class ColorUtilsTest: public UnitTest
    {
    public:
        ColorUtilsTest();
        virtual void Run();
    private:
        void Test32BitColwert();
        void TestFormatToString();
        static bool Supports32BitColwert(ColorUtils::Format colorFormat);
        static bool SupportsPngColwert(ColorUtils::Format colorFormat);
        Random m_Random;
    };
    ADD_UNIT_TEST(ColorUtilsTest);

    //----------------------------------------------------------------
    //! \brief Constructor
    //!
    ColorUtilsTest::ColorUtilsTest() :
        UnitTest("ColorUtilsTest")
    {
        m_Random.SeedRandom(static_cast<UINT32>(Xp::GetWallTimeUS()));
    }

    //----------------------------------------------------------------
    //! \brief Run all ColorUtils tests
    //!
    /* virtual */ void ColorUtilsTest::Run()
    {
        Test32BitColwert();
        TestFormatToString();
    }

    //----------------------------------------------------------------
    //! \brief Test the 32-bit ColorUtils::Colwert function
    //!
    //! This test loops through all supported pairs of color formats,
    //! and uses two techniques to colwert a random color from one
    //! format to the other:
    //! * Direct colwersion via the 32-bit Colwert() function.
    //! * Indirect colwersion via srcFmt => PNG => dstFmt.
    //! The test fails if the results are different.
    //!
    void ColorUtilsTest::Test32BitColwert()
    {
        ColorUtils::PNGFormat pngFmt = ColorUtils::C32C32C32C32;
        vector<char> pngData(PixelBytes(pngFmt));

        for (ColorUtils::Format srcFmt = static_cast<ColorUtils::Format>(0);
             srcFmt != ColorUtils::Format_NUM_FORMATS;
             srcFmt = static_cast<ColorUtils::Format>(srcFmt + 1))
        {
            if (!Supports32BitColwert(srcFmt) || !SupportsPngColwert(srcFmt))
                continue;
            UINT32 maxSrcData;
            if (srcFmt == ColorUtils::X8R8G8B8)
                maxSrcData = 0xffffff;
            else
                maxSrcData = 0xffffffff >> (32 - PixelBits(srcFmt));

            for (ColorUtils::Format dstFmt = static_cast<ColorUtils::Format>(0);
                 dstFmt != ColorUtils::Format_NUM_FORMATS;
                 dstFmt = static_cast<ColorUtils::Format>(dstFmt + 1))
            {
                if (!Supports32BitColwert(dstFmt) ||
                    !SupportsPngColwert(dstFmt))
                {
                    continue;
                }

                const UINT32 srcData = m_Random.GetRandom(0, maxSrcData);
                const UINT32 dstData1 =
                    ColorUtils::Colwert(srcData, srcFmt, dstFmt);
                UINT32 dstData2 = 0;
                ColorUtils::Colwert(reinterpret_cast<const char*>(&srcData),
                                    srcFmt, &pngData[0], pngFmt);
                ColorUtils::Colwert(&pngData[0], pngFmt,
                                    reinterpret_cast<char*>(&dstData2), dstFmt);
                if (dstData1 != dstData2)
                {
                    Printf(Tee::PriHigh,
                           "ERROR: ColorUtils::Colwert() failed on %s => %s:\n",
                           ColorUtils::FormatToString(srcFmt).c_str(),
                           ColorUtils::FormatToString(dstFmt).c_str());
                    Printf(Tee::PriHigh,
                           " - Direct colwert: 0x%08x => 0x%08x\n",
                           srcData, dstData1);
                    Printf(Tee::PriHigh, " - PNG colwert: 0x%08x =>", srcData);
                    for (UINT32 ii = 0; ii < pngData.size(); ++ii)
                    {
                        Printf(Tee::PriHigh, "%s%02x",
                                (ii % 4) ? "" : " ",
                                pngData[ii] & 0x00ff);
                    }
                    Printf(Tee::PriHigh, " => 0x%08x\n", dstData2);
                }
                UNIT_ASSERT_EQ(dstData1, dstData2);
            }
        }
    }

    //----------------------------------------------------------------
    //! \brief Test ColorUtils::FormatToString()
    //!
    //! This test colwerts every color format to a string and back,
    //! and makes sure the result is the original format.
    //!
    void ColorUtilsTest::TestFormatToString()
    {
        for (ColorUtils::Format fmt = static_cast<ColorUtils::Format>(0);
             fmt != ColorUtils::Format_NUM_FORMATS;
             fmt = static_cast<ColorUtils::Format>(fmt + 1))
        {
            string fmtName = ColorUtils::FormatToString(fmt);
            UNIT_ASSERT_EQ(fmt, ColorUtils::StringToFormat(fmtName));
        }
    }

    //----------------------------------------------------------------
    //! Check whether a color format is support by the
    //! ColorUtils::Colwert() function that returns the new format as
    //! a 32-bit integer.
    //!
    /* static */ bool ColorUtilsTest::Supports32BitColwert
    (
        ColorUtils::Format colorFormat
    )
    {
        // Only support formats with 1-32 bits
        //
        const UINT32 pixelBits = ColorUtils::PixelBits(colorFormat);
        if (pixelBits < 1 || pixelBits > 32)
            return false;

        // Ad-hoc list of other unsupported formats
        //
        switch (colorFormat)
        {
            case ColorUtils::A1B5G5R5:
            case ColorUtils::A4B4G4R4:
            case ColorUtils::A4R4G4B4:
            case ColorUtils::A5B5G5R1:
            case ColorUtils::A8:
            case ColorUtils::A8B6x2G6x2R6x2:
            case ColorUtils::A8BL8GL8RL8:
            case ColorUtils::A8R6x2G6x2B6x2:
            case ColorUtils::A8RL8GL8BL8:
            case ColorUtils::A8Y8U8V8:
            case ColorUtils::AN8BN8GN8RN8:
            case ColorUtils::AS8BS8GS8RS8:
            case ColorUtils::AU2BU10GU10RU10:
            case ColorUtils::AU8BU8GU8RU8:
            case ColorUtils::AUDIOL16_AUDIOR16:
            case ColorUtils::B4G4R4A4:
            case ColorUtils::B5G5R5A1:
            case ColorUtils::B5G5R5X1:
            case ColorUtils::B5G6R5:
            case ColorUtils::B6G5R5:
            case ColorUtils::B8G8R8A8:
            case ColorUtils::B8G8R8G8:
            case ColorUtils::B8_G8_R8:
            case ColorUtils::BF10GF11RF11:
            case ColorUtils::CB8:
            case ColorUtils::CB8CR8:
            case ColorUtils::CPSTY8CPSTC8:
            case ColorUtils::CR8:
            case ColorUtils::CR8CB8:
            case ColorUtils::CR8YB8CB8YA8:
            case ColorUtils::E5B9G9R9_SHAREDEXP:
            case ColorUtils::G24R8:
            case ColorUtils::G4R4:
            case ColorUtils::G8B8G8R8:
            case ColorUtils::G8R24:
            case ColorUtils::G8R8:
            case ColorUtils::GN8RN8:
            case ColorUtils::GS8RS8:
            case ColorUtils::GU8RU8:
            case ColorUtils::I1:
            case ColorUtils::I2:
            case ColorUtils::I4:
            case ColorUtils::R1:
            case ColorUtils::R32:
            case ColorUtils::R32_G32_B32:
            case ColorUtils::R4G4B4A4:
            case ColorUtils::R5G5B5A1:
            case ColorUtils::R5G5B5X1:
            case ColorUtils::R8:
            case ColorUtils::RF16:
            case ColorUtils::RF16_GF16:
            case ColorUtils::RF32:
            case ColorUtils::RN16:
            case ColorUtils::RN16_GN16:
            case ColorUtils::RN8:
            case ColorUtils::RS16:
            case ColorUtils::RS16_GS16:
            case ColorUtils::RS32:
            case ColorUtils::RS8:
            case ColorUtils::RU16:
            case ColorUtils::RU16_GU16:
            case ColorUtils::RU32:
            case ColorUtils::RU8:
            case ColorUtils::S8Z24:
            case ColorUtils::U8:
            case ColorUtils::UE8YO8VE8YE8:
            case ColorUtils::V8:
            case ColorUtils::V8U8:
            case ColorUtils::V8Z24:
            case ColorUtils::V8Z24__COV4R12V:
            case ColorUtils::V8Z24__COV8R24V:
            case ColorUtils::VOID8:
            case ColorUtils::X1B5G5R5:
            case ColorUtils::X1R5G5B5:
            case ColorUtils::X4V4Z24__COV4R4V:
            case ColorUtils::X4V4Z24__COV8R8V:
            case ColorUtils::X8B8G8R8:
            case ColorUtils::X8BL8GL8RL8:
            case ColorUtils::X8RL8GL8BL8:
            case ColorUtils::X8Z24:
            case ColorUtils::Y8_VIDEO:
            case ColorUtils::YB8CR8YA8CB8:
            case ColorUtils::YO8UE8YE8VE8:
            case ColorUtils::Z24:
            case ColorUtils::ZF32:
            case ColorUtils::Y8_U8__Y8_V8_N422:
            case ColorUtils::U8_Y8__V8_Y8_N422:
            case ColorUtils::Y8___U8V8_N444:
            case ColorUtils::Y8___U8V8_N422:
            case ColorUtils::Y8___U8V8_N422R:
            case ColorUtils::Y8___V8U8_N420:
            case ColorUtils::Y8___U8___V8_N444:
            case ColorUtils::Y8___U8___V8_N420:
            case ColorUtils::Y10___U10V10_N444:
            case ColorUtils::Y10___U10V10_N422:
            case ColorUtils::Y10___U10V10_N422R:
            case ColorUtils::Y10___V10U10_N420:
            case ColorUtils::Y10___U10___V10_N444:
            case ColorUtils::Y10___U10___V10_N420:
                return false;
            default:
                break;
        }
        return true;
    }

    //----------------------------------------------------------------
    //! Check whether a color format is support by the
    //! ColorUtils::Colwert() functions that colwert between color
    //! formats and PNG formats.
    //!
    /* static */ bool ColorUtilsTest::SupportsPngColwert
    (
        ColorUtils::Format colorFormat
    )
    {
        // Ad-hoc list of unsupported formats
        //
        switch (colorFormat)
        {
            case ColorUtils::A1B5G5R5:
            case ColorUtils::A2V10Y10U10:
            case ColorUtils::A4B4G4R4:
            case ColorUtils::A4R4G4B4:
            case ColorUtils::A5B5G5R1:
            case ColorUtils::A8B6x2G6x2R6x2:
            case ColorUtils::A8BL8GL8RL8:
            case ColorUtils::A8R6x2G6x2B6x2:
            case ColorUtils::A8RL8GL8BL8:
            case ColorUtils::A8Y8U8V8:
            case ColorUtils::AN8BN8GN8RN8:
            case ColorUtils::AS8BS8GS8RS8:
            case ColorUtils::ASTC_2D_10X10:
            case ColorUtils::ASTC_2D_10X5:
            case ColorUtils::ASTC_2D_10X6:
            case ColorUtils::ASTC_2D_10X8:
            case ColorUtils::ASTC_2D_12X10:
            case ColorUtils::ASTC_2D_12X12:
            case ColorUtils::ASTC_2D_4X4:
            case ColorUtils::ASTC_2D_5X4:
            case ColorUtils::ASTC_2D_5X5:
            case ColorUtils::ASTC_2D_6X5:
            case ColorUtils::ASTC_2D_6X6:
            case ColorUtils::ASTC_2D_8X5:
            case ColorUtils::ASTC_2D_8X6:
            case ColorUtils::ASTC_2D_8X8:
            case ColorUtils::ASTC_SRGB_2D_10X10:
            case ColorUtils::ASTC_SRGB_2D_10X5:
            case ColorUtils::ASTC_SRGB_2D_10X6:
            case ColorUtils::ASTC_SRGB_2D_10X8:
            case ColorUtils::ASTC_SRGB_2D_12X10:
            case ColorUtils::ASTC_SRGB_2D_12X12:
            case ColorUtils::ASTC_SRGB_2D_4X4:
            case ColorUtils::ASTC_SRGB_2D_5X4:
            case ColorUtils::ASTC_SRGB_2D_5X5:
            case ColorUtils::ASTC_SRGB_2D_6X5:
            case ColorUtils::ASTC_SRGB_2D_6X6:
            case ColorUtils::ASTC_SRGB_2D_8X5:
            case ColorUtils::ASTC_SRGB_2D_8X6:
            case ColorUtils::ASTC_SRGB_2D_8X8:
            case ColorUtils::ETC2_RGB:
            case ColorUtils::ETC2_RGB_PTA:
            case ColorUtils::ETC2_RGBA:
            case ColorUtils::EAC:
            case ColorUtils::EACX2:
            case ColorUtils::AU2BU10GU10RU10:
            case ColorUtils::AU8BU8GU8RU8:
            case ColorUtils::AUDIOL16_AUDIOR16:
            case ColorUtils::AUDIOL32_AUDIOR32:
            case ColorUtils::B4G4R4A4:
            case ColorUtils::B5G5R5A1:
            case ColorUtils::B5G5R5X1:
            case ColorUtils::B5G6R5:
            case ColorUtils::B6G5R5:
            case ColorUtils::B8G8R8A8:
            case ColorUtils::B8G8R8G8:
            case ColorUtils::BC6H_SF16:
            case ColorUtils::BC6H_UF16:
            case ColorUtils::BC7U:
            case ColorUtils::BF10GF11RF11:
            case ColorUtils::CB8:
            case ColorUtils::CB8CR8:
            case ColorUtils::CPST8:
            case ColorUtils::CPSTY8CPSTC8:
            case ColorUtils::CR8:
            case ColorUtils::CR8CB8:
            case ColorUtils::LWFMT_NONE:
            case ColorUtils::DXN1:
            case ColorUtils::DXN2:
            case ColorUtils::DXT1:
            case ColorUtils::DXT23:
            case ColorUtils::DXT45:
            case ColorUtils::E5B9G9R9_SHAREDEXP:
            case ColorUtils::G24R8:
            case ColorUtils::G4R4:
            case ColorUtils::G8B8G8R8:
            case ColorUtils::G8R24:
            case ColorUtils::G8R8:
            case ColorUtils::GN8RN8:
            case ColorUtils::GS8RS8:
            case ColorUtils::GU8RU8:
            case ColorUtils::I1:
            case ColorUtils::I2:
            case ColorUtils::I4:
            case ColorUtils::R1:
            case ColorUtils::R32:
            case ColorUtils::R32_B24G8:
            case ColorUtils::R32_G32:
            case ColorUtils::R32_G32_B32:
            case ColorUtils::R32_G32_B32_A32:
            case ColorUtils::R4G4B4A4:
            case ColorUtils::R5G5B5A1:
            case ColorUtils::R5G5B5X1:
            case ColorUtils::RF16_GF16:
            case ColorUtils::RF16_GF16_BF16_X16:
            case ColorUtils::RF32_GF32:
            case ColorUtils::RF32_GF32_BF32_X32:
            case ColorUtils::RN16:
            case ColorUtils::RN16_GN16:
            case ColorUtils::RN16_GN16_BN16_AN16:
            case ColorUtils::RS16:
            case ColorUtils::RS16_GS16:
            case ColorUtils::RS32:
            case ColorUtils::RS32_GS32:
            case ColorUtils::RS32_GS32_BS32_AS32:
            case ColorUtils::RS32_GS32_BS32_X32:
            case ColorUtils::RU16:
            case ColorUtils::RU16_GU16:
            case ColorUtils::RU32_GU32:
            case ColorUtils::RU32_GU32_BU32_AU32:
            case ColorUtils::RU32_GU32_BU32_X32:
            case ColorUtils::S8Z24:
            case ColorUtils::U8:
            case ColorUtils::UE10Z6_YE10Z6_VE10Z6_YO10Z6:
            case ColorUtils::UE16_YE16_VE16_YO16:
            case ColorUtils::UE8YO8VE8YE8:
            case ColorUtils::V8:
            case ColorUtils::V8U8:
            case ColorUtils::V8Z24:
            case ColorUtils::V8Z24__COV4R12V:
            case ColorUtils::V8Z24__COV8R24V:
            case ColorUtils::VE8YO8UE8YE8:
            case ColorUtils::VOID32:
            case ColorUtils::VOID8:
            case ColorUtils::X1B5G5R5:
            case ColorUtils::X1R5G5B5:
            case ColorUtils::X4V4Z24__COV4R4V:
            case ColorUtils::X4V4Z24__COV8R8V:
            case ColorUtils::X8B8G8R8:
            case ColorUtils::X8BL8GL8RL8:
            case ColorUtils::X8RL8GL8BL8:
            case ColorUtils::X8Z24:
            case ColorUtils::X8Z24_X16V8S8:
            case ColorUtils::X8Z24_X16V8S8__COV4R12V:
            case ColorUtils::X8Z24_X16V8S8__COV8R24V:
            case ColorUtils::X8Z24_X20V4S8__COV4R4V:
            case ColorUtils::X8Z24_X20V4S8__COV8R8V:
            case ColorUtils::Y8_VIDEO:
            case ColorUtils::YE10Z6_UE10Z6_YO10Z6_VE10Z6:
            case ColorUtils::YE16_UE16_YO16_VE16:
            case ColorUtils::YO8UE8YE8VE8:
            case ColorUtils::YO8VE8YE8UE8:
            case ColorUtils::Z8R8G8B8:
            case ColorUtils::ZF32:
            case ColorUtils::ZF32_X16V8S8:
            case ColorUtils::ZF32_X16V8S8__COV4R12V:
            case ColorUtils::ZF32_X16V8S8__COV8R24V:
            case ColorUtils::ZF32_X16V8X8:
            case ColorUtils::ZF32_X16V8X8__COV4R12V:
            case ColorUtils::ZF32_X16V8X8__COV8R24V:
            case ColorUtils::ZF32_X20V4S8__COV4R4V:
            case ColorUtils::ZF32_X20V4S8__COV8R8V:
            case ColorUtils::ZF32_X20V4X8__COV4R4V:
            case ColorUtils::ZF32_X20V4X8__COV8R8V:
            case ColorUtils::ZF32_X24S8:
            case ColorUtils::Y12___U12V12_N444:
            case ColorUtils::Y12___U12V12_N422:
            case ColorUtils::Y12___U12V12_N422R:
            case ColorUtils::Y12___V12U12_N420:
            case ColorUtils::Y12___U12___V12_N444:
            case ColorUtils::Y12___U12___V12_N420:
                return false;
            default:
                break;
        }
        return true;
    }
}
