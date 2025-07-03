/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2017, 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include <string>
#include "gpu/tests/gputest.h"
#include "core/include/display.h"

class IDTPrivateImpl;
struct TestArguments;

typedef std::string DisplaysArg;
typedef std::string DisplaysExArg;
typedef std::string ResolutionArg;
typedef Display::ORColorSpace ORColorSpace;
typedef LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP ORPixelDepthBpp;

class IDT : public GpuTest
{
public:
    IDT();

    ~IDT();

    bool IsSupported() override;

    RC Setup() override;

    RC Run() override;

    RC Cleanup() override;

    SETGET_PROP(Image, std::string);
    SETGET_PROP(RtImage, std::string);
    SETGET_PROP(Displays, std::string);
    SETGET_PROP(Resolution, ResolutionArg);
    SETGET_PROP(DisplaysEx, DisplaysExArg);
    SETGET_PROP(SkipHDCP, bool);
    SETGET_PROP(RandomPrompts, bool);
    SETGET_PROP(EnableDMA, bool);
    SETGET_PROP(SkipVisualText, bool);
    GET_PROP(EnableDVSRun, bool);
    SET_PROP_LWSTOM(EnableDVSRun, bool);
    SETGET_PROP(PromptTimeout, UINT32);
    SETGET_PROP(EnableBkgndMode, bool);
    SETGET_PROP(AssumeSuccessOnPromptTimeout, bool);
    SETGET_PROP(TestUnsupportedDisplays, bool);
    SETGET_PROP(SkipSingleDisplays, bool);

    SETGET_PROP_LWSTOM(ORPixelDepth, UINT32);
    SETGET_PROP_LWSTOM(ColorSpace, UINT32);

    RC SetTestArguments(const TestArguments& testArguments);
private:
    std::string m_Image;
    std::string m_RtImage;
    DisplaysArg m_Displays;
    ResolutionArg m_Resolution;
    DisplaysExArg m_DisplaysEx;
    bool m_SkipHDCP = true;
    bool m_RandomPrompts = false;
    bool m_EnableDMA = true;
    bool m_SkipVisualText = false;
    bool m_EnableDVSRun = false;
    UINT32 m_PromptTimeout = 0;
    bool m_EnableBkgndMode = false;
    bool m_AssumeSuccessOnPromptTimeout = false;
    bool m_TestUnsupportedDisplays = false;
    bool m_SkipSingleDisplays = false;
    ORPixelDepthBpp m_ORPixelDepth = LW_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_DEFAULT;
    ORColorSpace m_ColorSpace = ORColorSpace::RGB;

    std::unique_ptr<IDTPrivateImpl> m_pPrivateImpl;

    std::unique_ptr<TestArguments> m_pTestArguments;
    RC FillTestArguments();
    const TestArguments& GetTestArguments();
};
