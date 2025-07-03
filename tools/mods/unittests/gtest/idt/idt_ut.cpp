/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gmock/gmock.h"
#include "gpu/tests/idt/idt.h"
#include "gpu/tests/idt/idt_private_impl.h"
#include "gpu/utility/surffill.h"
#include "gpu/display/lwdisplay/lw_disp_c5.h"

using namespace testing;
using namespace SurfaceUtils;

class SurfaceFillStub : public SurfaceFill
{
public:
    SurfaceFillStub(): SurfaceFill(false){}
    RC Cleanup() override
    {
        return OK;
    }
    RC PrepareSurface
    (
        SurfaceUtils::SurfaceFill::SurfaceId, 
        const Display::Mode&, ColorUtils::Format
    ) override
    {
        return OK;
    }
    RC PrepareSurface
    (
         SurfaceId surfaceId,
         const Display::Mode& resolutionMode,
         UINT32 surfFieldType,
         ColorUtils::Format forcedColorFormat = ColorUtils::LWFMT_NONE
    ) override
    {
        return RC::OK;
    }
    RC PrepareSurface
    (
        SurfaceId surfaceId,
        const Display::Mode& resolutionMode,
        ColorUtils::Format forcedColorFormat,
        ColorUtils::YCrCbType m_YCrCbType,
        Surface2D::Layout layout,
        UINT32 logBlockWidth,
        UINT32 logBlockHeight,
        UINT32 surfFieldType, 
        Surface2D::InputRange inputRange,
        unique_ptr<Surface2D> pRefSysmemSurf
    ) override
    {
        return OK;
    }

    RC Reserve(const vector<SurfaceId> &  sIds) override
    {
        return OK;
    }

    RC Insert(const SurfaceId sId, unique_ptr<Surface2D> pSurf) override
    {
        return OK;
    }

    RC Insert
    (
        const SurfaceId sId,
        unique_ptr<Surface2D> pFbSurf,
        shared_ptr<Surface2D> pSysSurf
    ) override
    {
        return OK;
    }

    void Erase(const SurfaceId sId) override
    {
    }

    RC CopySurface(SurfaceId surfaceId) override
    {
        return OK;
    }
    Surface2D* GetSurface(SurfaceId surfaceId) override
    {
        return nullptr;
    }
    Surface2D* GetImage(SurfaceId surfaceId) override
    {
        return nullptr;
    }
    Surface2D* operator[](SurfaceId sId) override
    {
        return nullptr;
    }
    RC FreeAllSurfaces() override
    {
        return OK;
    }
};

class IDTPrivateImplMock : public IDTPrivateImpl
{
public:
    IDTPrivateImplMock(): IDTPrivateImpl(nullptr, nullptr){}
    MOCK_METHOD1(SetAllDisplaysToBeTested, RC(bool value));
    MOCK_METHOD2(AreAllDisplaysToBeTested, bool
    (
        const DisplayID& displayID, 
        const ResolutionInfo& resolutionInfo
    ));
    MOCK_METHOD0(GetPrimaryDisplay, DisplayID());
    MOCK_METHOD1(SetCommonResolutionInfo, RC(const ResolutionInfo& resolutionInfo));
};

class SurfaceFillMock : public SurfaceFillStub
{
public:
    MOCK_METHOD1(IsSupportedImage, bool(const std::string& image));
};

class IDTJsonParserTester : public Test
{
public:
    IDTJsonParser idtJsonParser;
    NiceMock<IDTPrivateImplMock> pimplMock;
    SurfaceFillMock surfaceFillMock;
    TestArguments testArguments;

    virtual void SetUp()
    {
        testArguments.displays = "";
        testArguments.image = "diag_stripes";
        testArguments.resolution = "";
        testArguments.rtImage = "";
        testArguments.skipHDCP = true;
        testArguments.enableDVSRun = false;
        testArguments.testUnsupportedDisplays = false;
        testArguments.skipSingleDisplays = false;
        testArguments.assumeSuccessOnPromptTimeout = false;
        testArguments.enableDMA = true;
        testArguments.promptTimeoutMs = 0;
        testArguments.isInitialized = false;
        testArguments.randomPrompts = false;
        testArguments.oRPixelDepth = LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_DEFAULT;
        testArguments.colorSpace = ORColorSpace::RGB;
        testArguments.displaysEx = "[{\"0x200\": [1024, 768, 32, 60]}, {\"0x1000\": \"max\"}]";
        testArguments.hdcpTimeout = 0;
    }
};

TEST_F(IDTJsonParserTester, ValidatesDisplaysExParameter)
{
    TestArguments testArgumentsValidDisplaysEx;
    testArgumentsValidDisplaysEx  = testArguments;
     
    EXPECT_CALL(surfaceFillMock, IsSupportedImage(_)).Times(1).WillOnce(Return(true));
    idtJsonParser.Init(&pimplMock, &surfaceFillMock);
    ASSERT_THAT(idtJsonParser.ValidateJson(testArgumentsValidDisplaysEx), true);
}

TEST_F(IDTJsonParserTester, ValidatesDisplaysParameter)
{
    TestArguments testArgumentsValidDisplays;
      
    //set params for validdisplaystest
    testArguments.displays = "[\"all\"]";
    testArguments.resolution = "native";
    testArguments.displaysEx = "";
    testArgumentsValidDisplays = testArguments;

    EXPECT_CALL(surfaceFillMock, IsSupportedImage(_)).Times(1).WillOnce(Return(true));
    idtJsonParser.Init(&pimplMock, &surfaceFillMock);
    ASSERT_THAT(idtJsonParser.ValidateJson(testArgumentsValidDisplays), true);
}

TEST_F(IDTJsonParserTester, IlwalidatesDisplaysExParameter)
{
     TestArguments testArgumentsIlwalidDisplaysEx;
  
     //set param for ilwaliddisplaysex
     testArguments.displaysEx = "[{\"0x200\": [1024, 768, 32, 60]} {\"0x1000\": \"max\"}]";
     testArgumentsIlwalidDisplaysEx = testArguments;

     EXPECT_CALL(surfaceFillMock, IsSupportedImage(_)).Times(1).WillOnce(Return(true));
     idtJsonParser.Init(&pimplMock, &surfaceFillMock);
     ASSERT_THAT(idtJsonParser.ValidateJson(testArgumentsIlwalidDisplaysEx), false);
}

TEST_F(IDTJsonParserTester, ValidatesIlwalidImageParameter)
{
    TestArguments testArgumentsIlwalidImage;

    //set invalid image parameter
    testArguments.image = "xyzw";
    testArgumentsIlwalidImage  = testArguments;
     
    EXPECT_CALL(surfaceFillMock, IsSupportedImage(_)).Times(1).WillOnce(Return(false));
    idtJsonParser.Init(&pimplMock, &surfaceFillMock);
    ASSERT_THAT(idtJsonParser.ValidateJson(testArgumentsIlwalidImage), false);
}

TEST_F(IDTJsonParserTester, ValidatesResolutionParameter)
{
    TestArguments testArgumentsValidResolution;

    //set valid resolution parameter
    testArguments.resolution = "max";
    testArguments.displaysEx = "";
    testArgumentsValidResolution  = testArguments;
     
    EXPECT_CALL(surfaceFillMock, IsSupportedImage(_)).Times(3).WillRepeatedly(Return(true));
    idtJsonParser.Init(&pimplMock, &surfaceFillMock);
    ASSERT_THAT(idtJsonParser.ValidateJson(testArgumentsValidResolution), true);

    //set valid resolution parameter
    testArguments.resolution = "native";
    testArgumentsValidResolution  = testArguments;

    ASSERT_THAT(idtJsonParser.ValidateJson(testArgumentsValidResolution), true);

    //set valid resolution parameter
    testArguments.resolution = "[1024, 768, 32, 60]";
    testArgumentsValidResolution  = testArguments;

    ASSERT_THAT(idtJsonParser.ValidateJson(testArgumentsValidResolution), true);
}

TEST_F(IDTJsonParserTester, ValidatesResolutionParameterForIlwalidInputs)
{
    TestArguments testArgumentsIlwalidResolution;

    //set invalid resolution parameter
    testArguments.resolution = "xyz";
    testArguments.displaysEx = "";
    testArgumentsIlwalidResolution  = testArguments;

    EXPECT_CALL(surfaceFillMock, IsSupportedImage(_)).Times(3).WillRepeatedly(Return(true));
    idtJsonParser.Init(&pimplMock, &surfaceFillMock);
    ASSERT_THAT(idtJsonParser.ValidateJson(testArgumentsIlwalidResolution), false);

    //set invalid resolution parameter
    testArguments.resolution = "[1024, 768, 32]";
    testArgumentsIlwalidResolution  = testArguments;

    ASSERT_THAT(idtJsonParser.ValidateJson(testArgumentsIlwalidResolution), false);

    //set invalid resolution parameter
    testArguments.resolution = "[1024, 768, \"xyz\", 60]";
    testArgumentsIlwalidResolution  = testArguments;

    ASSERT_THAT(idtJsonParser.ValidateJson(testArgumentsIlwalidResolution), false);
}

