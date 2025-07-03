/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2011,2016-2017,2019 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_modeset.cpp
//! \brief Do a mode set on an EVO family display
//!
//! This test's purpose in life is to do a mode set as a basic end-to-end check
//
//! A secondary goal is to have a pre-canned test that can do a bunch of mode
//! sets quickly.  May not fully flesh this out the first time.
//!
//! \sa SorLoopback
//! \sa EvoOvrl

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/color.h"
#include "core/include/display.h"
#include "gpu/display/evo_disp.h"
#include "gpu/utility/surf2d.h"
#include "core/include/golden.h"
#include "gpu/include/gpugoldensurfaces.h"
#include "gpu/include/gpudev.h"
#include "core/include/memcheck.h"

class ModeSetTest : public RmTest
{
public:
    ModeSetTest();
    virtual ~ModeSetTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    Surface2D          m_PrimarySurface;
    Goldelwalues       m_Golden;
    GpuGoldenSurfaces *m_pGGSurfs;
    Display*           m_pDisplay;
};

//! \brief Init everything to safely free-able values
//!
//! \sa Setup
//------------------------------------------------------------------------------
ModeSetTest::ModeSetTest() : m_pGGSurfs(nullptr), m_pDisplay(nullptr)
{
    SetName("ModeSetTest");
}

//! \brief Nothing to do here
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
ModeSetTest::~ModeSetTest()
{

}

//! \brief Only want to run on EVO displays
//!
//! No point running on a NULL display and there's a decent chance we'll want
//! to fiddle with EVO specific things below.
//!
//! The test as it is lwrrently written is NOT EVO specific and *could* run on
//! CLASS_XX7C related displays.  However, I intend to extend the test in the
//! future and make use of EVO specific functionality.  Furthermore, the test
//! assumes block linear is supported and, while not specific to EVO, it is
//! specific to GPUs (Tesla and later) that also happen to support EVO.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string ModeSetTest::IsTestSupported()
{
    m_pDisplay = GetDisplay();
    switch(m_pDisplay->GetDisplayClassFamily())
    {
        case Display::NULLDISP:
            return "No point running a mode set test on a NULL display";
        case Display::CLASS_XX7C:
            return "ModeSetTest doesn't support legacy display classes (EVO only)";
        case Display::EVO:
            return RUN_RMTEST_TRUE;
        default:
            MASSERT(!"Unknown display class family");
            break;
    }

    return "Unknown display class family";
}

//! \brief Allocate primary surface and setup golden values
//!
//! Fails if we can't init from JS, alloc primary surface, or setup golden
//! values properly.
//------------------------------------------------------------------------------
RC ModeSetTest::Setup()
{
    RC rc;
    GpuTestConfiguration *myTstCfg = GetTestConfiguration();

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    CHECK_RC(m_Golden.InitFromJs(GetJSObject()));

    m_pDisplay = GetDisplay();

    // For now, just alloc a single primary surface.  Later we'll deal with
    // different modes
    m_PrimarySurface.SetDisplayable(true);
    m_PrimarySurface.SetWidth(myTstCfg->DisplayWidth());
    m_PrimarySurface.SetHeight(myTstCfg->DisplayHeight());
    m_PrimarySurface.SetColorFormat(
        ColorUtils::ColorDepthToFormat(myTstCfg->DisplayDepth()));
    m_PrimarySurface.SetAddressModel(Memory::Paging);

    // Could hook this to a fancy picker.
    // We're cheating off the fact that the chip supports EVO (thus
    // we know it supports BL)
    m_PrimarySurface.SetLayout(Surface2D::BlockLinear);

    // Do I need to set block width / block height?

    CHECK_RC(m_PrimarySurface.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_PrimarySurface.FillPattern(1, 1, "bars", "primaries", "vertical"));
    m_pGGSurfs = new GpuGoldenSurfaces(GetBoundGpuDevice());
    //
    // "C" for "Color".  We take a HW CRC anyway, we just need to tell the
    // golden surface which display we're interested in CRC'ing
    m_pGGSurfs->AttachSurface(&m_PrimarySurface, "C", m_pDisplay->Selected());
    CHECK_RC(m_Golden.SetSurfaces(m_pGGSurfs));

    return rc;
}

//! \brief Do the mode set and callwlate a CRC
//!
//! This leverages common MODS code to do a mode set and (depending on JS
//! settings) store or check the CRC of the primary surface.  By default, this
//! test will do a display CRC rather than a SW CRC.  (As of the writing of
//! this comment.  Check Golden.Codes to be certain)
//------------------------------------------------------------------------------
RC ModeSetTest::Run()
{
    RC rc;
    LwRm::Handle hBaseChan;
    GpuTestConfiguration *myTstCfg = GetTestConfiguration();

    // Do we need disable/enable UI calls here for real HW?

    CHECK_RC(m_pDisplay->SetMode(
        myTstCfg->DisplayWidth(),
        myTstCfg->DisplayHeight(),
        myTstCfg->DisplayDepth(),
        myTstCfg->RefreshRate()));
    if (OK == m_pDisplay->GetEvoDisplay()->GetBaseChannelHandle(m_pDisplay->Selected(),&hBaseChan))
    {
        CHECK_RC(m_PrimarySurface.BindIsoChannel(hBaseChan));
    }
    CHECK_RC(m_pDisplay->SetImage(&m_PrimarySurface));

    // Store or check the CRC (depending on whether we're generating or
    // checking golden values
    CHECK_RC(m_Golden.Run());

    return rc;
}

//! \brief Release resources
//!
//! Tell the display not to scan out
//------------------------------------------------------------------------------
RC ModeSetTest::Cleanup()
{
    m_Golden.ClearSurfaces();
    delete m_pGGSurfs;
    m_pGGSurfs = NULL;

    m_PrimarySurface.Free();

    return OK;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

// None

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.  Don't worry about the details
//! here, you probably don't care.  :-)
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(ModeSetTest, RmTest,
    "Mode Set example test");

