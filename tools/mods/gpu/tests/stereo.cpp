/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   stereo.cpp
 * @brief  Implementation of stereo test.
 *
 */

#include "gputest.h"
#include "gpu/include/gpudev.h"
#include "core/include/display.h"
#include "lwos.h"
#include "core/include/xp.h"
#include "core/include/utility.h"
#include "gpu/utility/gpuutils.h"
#include "core/include/platform.h"
#include "core/include/cnslmgr.h"
#include "core/include/console.h"
#include "gpu/include/displaycleanup.h"

class StereoTest: public GpuTest
{
  private:
      GpuTestConfiguration *    m_pTestConfig;

  public:
    //
    // Methods
    //
    StereoTest();

    // Initialize the test properties.
    RC InitProperties();

    // Run the stereo test.
    RC Run();

    virtual bool IsSupported();
};

//----------------------------------------------------------------------------

/**
 * StereoTest script interface.
 */

JS_CLASS_INHERIT(StereoTest, GpuTest, "StereoTest object.");

//----------------------------------------------------------------------------

StereoTest::StereoTest()
{
    SetName("StereoTest");
    m_pTestConfig = GetTestConfiguration();
}

bool StereoTest::IsSupported()
{
    return (GetDisplay()->GetDisplayClassFamily() == Display::EVO);
}

RC StereoTest::InitProperties()
{
    return OK;
} // StereoTest::InitProperties

RC StereoTest::Run()
{
    RC rc;
    Display *pDisplay = GetBoundGpuDevice()->GetDisplay();
    UINT32 Selected = pDisplay->Selected();


    // Get all the JS properties.
    CHECK_RC(InitProperties());

    CHECK_RC(Platform::DisableUserInterface(
                 m_pTestConfig->DisplayWidth(),
                 m_pTestConfig->DisplayHeight(),
                 m_pTestConfig->DisplayDepth(),
                 m_pTestConfig->RefreshRate(),
                 1, 1, 2, GetBoundGpuDevice()));

    // Cleanup
    Utility::CleanupOnReturn<void, RC>
            EnableUI(&Platform::EnableUserInterface);
    DisplayCleanup::EnableStereoCleanup
            DisableStereo(pDisplay, Selected, false);

    { // Cleanup

        // Clear the screen.
        Surface2D Image;
        Image.SetWidth(m_pTestConfig->DisplayWidth());
        Image.SetHeight(m_pTestConfig->DisplayHeight());
        Image.SetColorFormat(ColorUtils::ColorDepthToFormat(m_pTestConfig->DisplayDepth()));
        Image.SetLayout(Surface2D::Pitch);
        Image.SetDisplayable(true);
        Image.SetType(LWOS32_TYPE_IMAGE);
        Image.SetAddressModel(Memory::Paging);
        CHECK_RC(Image.Alloc(GetBoundGpuDevice()));
        CHECK_RC(Image.Fill(0, GetBoundGpuDevice()->GetDisplaySubdeviceInst()));
        CHECK_RC(Image.Map(GetBoundGpuDevice()->GetDisplaySubdeviceInst()));

        CHECK_RC(pDisplay->EnableStereo(Selected, true));

        CHECK_RC(pDisplay->SetImage(&Image));

        // Ask the user if he sees the stereo transmitter is active.
        Utility::FlushInputBuffer();
        GpuUtility::PutCharsHorizCentered(m_pTestConfig->DisplayWidth(),
                                          m_pTestConfig->DisplayHeight() / 2,
                                          "Is the stereo transmitter active (y/n)?",
                                          m_pTestConfig->DisplayWidth() / 512,
                                          0xFFFFFFFF,
                                          0,
                                          GpuUtility::DISPLAY_DEFAULT_PUTCHARS,
                                          GetBoundGpuSubdevice());
        char Answer;
        ConsoleManager::ConsoleContext consoleCtx;
        Console *pConsole = consoleCtx.AcquireRealConsole(true);
        do
        {
            Answer = tolower(pConsole->GetKey());
        } while ((Answer != 'n') && (Answer != 'y'));

        if ('n' == Answer)
            CHECK_RC(RC::BAD_STEREO_CONNECTOR);
    }

    return rc;
} // StereoTest::Run
