/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_suspendresumegpu.cpp
//! \brief Quick suspend/resume of GPU
//!
//! This test does the quick suspend of the GPU and then resume it immediately
//! Here we have call the suspend/resume of GPU directly through RM control diag class.
//! This test is design to run under simulation only.

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/display.h"
#include "core/include/gpu.h"
#include "core/include/platform.h"
#include "gpu/utility/gpuutils.h"

#include <string> // Only use <> for built-in C++ stuff
#include "gpu/perf/powermgmtutil.h"
#include "ctrl/ctrl208f.h" // LW20_SUBDEVICE_DIAG CTRL
#include "ctrl/ctrl2080/ctrl2080power.h"
#include "class/cl906f.h" // GF100_CHANNEL_GPFIFO
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/tests/rm/memsys/rmt_acrverify.h"
#include "rmlsfm.h"
#include "core/include/memcheck.h"
using namespace DTIUtils;

//Time delay between each successive iteration in microseconds
#define ITERATION_DELAY_USEC             1000000 // 1 secs.

//Time delay between suspend/resume of GPU in milliseconds
#define SUSPEND_TIME_MSEC                1000 // 1 sec.

//This is used for reset of GPU between standby and resume
#define PMC_ENABLE_REGISTER              0x00000200

class QuickSuspendResumeGPU : public RmTest
{
public:
    QuickSuspendResumeGPU();
    virtual ~QuickSuspendResumeGPU();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    RC SetGpuPowerOnOff(UINT32 action);
    RC SetGpuPrePostPowerOnOff(UINT32 action);
    RC WaitForGC6PlusIslandLoad();
    RC TestSuspendResume();

    SETGET_PROP(QsrIterations, UINT32);     //!< Grab JS variable QsrIterations
    SETGET_PROP(isModeGC6, bool);           //!< Grab JS variable QsrIterations
    SETGET_PROP(isModeRTD3GC6, bool);       //!< Grab JS variable QsrIterations
    SETGET_PROP(isModeD3Hot, bool);         //!< Grab JS variable QsrIterations,
                                            // must used with isModeRTD3GC6 and RM regkey RMForceRtd3D3Hot 0x1
    SETGET_PROP(isModeFGC6, bool);          //!< Grab JS variable QsrIterations
    SETGET_PROP(TestDisplay, bool);         //!< Grab JS variable QsrIterations
    SETGET_PROP(NoForceP8, bool);           //!< Grab JS variable QsrIterations

private:
    //@{
    //! Test specific variables
    PowerMgmtUtil  m_pwrUtil;
    UINT32         m_QsrIterations;
    bool           m_isModeGC6;
    bool           m_isModeRTD3GC6;
    bool           m_isModeD3Hot;
    bool           m_isModeFGC6;
    bool           m_TestDisplay;
    bool           m_NoForceP8;
    UINT32         m_initPState;
    Display       *m_pDisplay;
    GpuSubdevice  *m_pSubdevice;
    bool           m_isGC6Supported;
    bool           m_isRTD3GC6Supported;
    bool           m_isFGC6Supported;
    //@}

};

//! \brief QuickSuspendResumeGPU constructor
//!
//! Just does nothing except setting a name for the test..the actual
//! functionality is done by Setup()
//!
//! \sa Setup
//----------------------------------------------------------------------------
QuickSuspendResumeGPU::QuickSuspendResumeGPU()
{

    SetName("QuickSuspendResumeGPU");
    m_QsrIterations = 3;
    m_isModeGC6     = false;
    m_isModeRTD3GC6 = false;
    m_isModeD3Hot   = false;
    m_isModeFGC6    = false;
    m_TestDisplay   = false;
    m_NoForceP8     = false;
    m_pSubdevice    = NULL;
    m_pDisplay      = NULL;
}

//! \brief QuickSuspendResumeGPU destructor
//!
//! All resources should be freed in the Cleanup member function, so
//! destructors shouldn't do much.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
QuickSuspendResumeGPU::~QuickSuspendResumeGPU()
{

}

//! \brief IsTestSupported()
//!

//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string QuickSuspendResumeGPU::IsTestSupported()
{
    LwRmPtr pLwRm;
    RC      rc;

    m_pDisplay = GetDisplay();

    //
    // After CL#26188861 GetBoundGpuSubdevice() should not be called from within the test's constructor.
    // Because the device is bound to the test until after it's constructed.
    // Binding the device to the test is typically done immediately after construction,
    // like IsTestSupported, Setup, or Run.
    // IsTestSupported() is before Setup() so acquire device handle here.
    //
    if (m_pSubdevice == NULL)
    {
         m_pSubdevice = GetBoundGpuSubdevice();
    }

    if (m_isModeGC6 || m_isModeRTD3GC6 || m_isModeFGC6) // GC6 mode
    {
        //
        // Check if the VBIOS used has P8 which is required
        //
        // TODO: It is better to be able to loop through the PStates present
        //       and query if the pstate supports GC6. We can then go through
        //       each pstate that supports GC6 and run the test below.
        //       Lwrrently there is no control call support to query if a pstate
        //       supports GC6 though so we will leave this for later
        //
        if (!m_NoForceP8)
        {
            LW2080_CTRL_PERF_GET_PSTATES_INFO_PARAMS pstatesInfoParams;

            rc = pLwRm->ControlBySubdevice(m_pSubdevice,
                    LW2080_CTRL_CMD_PERF_GET_PSTATES_INFO,
                    &pstatesInfoParams, sizeof(LW2080_CTRL_PERF_GET_PSTATES_INFO_PARAMS));

            if (rc != OK)
            {
                return "Unable to obtain PState information, test require P8 with GC6 support";
            }

            if (!(pstatesInfoParams.pstates & LW2080_CTRL_PERF_PSTATES_P8))
            {
                return "Current VBIOS does not support P8, P8 with GC6 support is required to run this test";
            }
        }

        if (m_TestDisplay)
        {
            if ((m_pDisplay->GetDisplayClassFamily() == Display::EVO) ||
                (m_pDisplay->GetDisplayClassFamily() == Display::LWDISPLAY))
            {
                return RUN_RMTEST_TRUE;
            }

            return "EVO/LWDISPLAY Display class is not supported on current platform";
        }
        else
        {
            // The power features supported control call will check for GC6 supprt in Run()
            return RUN_RMTEST_TRUE;
        }
    }
    else // Suspend resume mode
    {
        if (GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_HAS_QUICK_SUSPEND_RESUME))
        {
            return RUN_RMTEST_TRUE;
        }
        else
        {
            return "GPUSUB_HAS_QUICK_SUSPEND_RESUME device feature not supported";
        }
    }
}

//! \brief Setup()
//!
//! This function basically allocates a client, device, subdevice and subdevice
//! diagonal handles.
//!
//! \return RC -> OK if everything is allocated, test-specific RC if something
//!               failed while selwring resources.
//!
//------------------------------------------------------------------------------
RC QuickSuspendResumeGPU::Setup()
{
    RC rc;
    LwRmPtr pLwRm;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    m_pDisplay = GetDisplay();

    //
    // After CL#26188861 GetBoundGpuSubdevice() should not be called from within the test's constructor.
    // Because the device is bound to the test until after it's constructed.
    // Binding the device to the test is typically done immediately after construction,
    // like IsTestSupported, Setup, or Run.
    //
    if (m_pSubdevice == NULL)
    {
         m_pSubdevice = GetBoundGpuSubdevice();
    }

    // Switch to P8 unless instructed not to
    if ((!m_NoForceP8) &&
        (m_isModeGC6 || m_isModeRTD3GC6 || m_isModeFGC6))
    {
        LW2080_CTRL_PERF_GET_LWRRENT_PSTATE_PARAMS getLwrrentPStateParams;
        LW2080_CTRL_PERF_SET_FORCE_PSTATE_PARAMS setForcePStateParams;

        // Get current pstate for retore later
        getLwrrentPStateParams.lwrrPstate = LW2080_CTRL_PERF_PSTATES_UNDEFINED;
        CHECK_RC(pLwRm->ControlBySubdevice(m_pSubdevice,
                 LW2080_CTRL_CMD_PERF_GET_LWRRENT_PSTATE,
                 &getLwrrentPStateParams, sizeof(getLwrrentPStateParams)));

        m_initPState = getLwrrentPStateParams.lwrrPstate;

        // Switch to P8
        Printf(Tee::PriHigh, "Switching to P8 before entering test, saving current pstate 0x%x\n", m_initPState);

        setForcePStateParams.forcePstate = LW2080_CTRL_PERF_PSTATES_P8;
        setForcePStateParams.fallback = LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR;

        CHECK_RC(pLwRm->ControlBySubdevice(m_pSubdevice,
                 LW2080_CTRL_CMD_PERF_SET_FORCE_PSTATE,
                 &setForcePStateParams, sizeof(setForcePStateParams)));
    }

    return rc;
}

RC QuickSuspendResumeGPU::TestSuspendResume()
{
    RC rc;
    UINT32 entryAction = LW2080_CTRL_GPU_POWER_ON_OFF_GC6_ENTER;
    UINT32 exitAction  = LW2080_CTRL_GPU_POWER_ON_OFF_GC6_EXIT;
    UINT32 gpio;
    string gc6Type = m_isModeGC6?"GC6":(m_isModeFGC6?"FastGC6":"RTD3");

    if (m_isModeFGC6)
    {
        entryAction = LW2080_CTRL_GPU_POWER_ON_OFF_FAST_GC6_ENTER;
        exitAction  = LW2080_CTRL_GPU_POWER_ON_OFF_FAST_GC6_EXIT;
    }

    if (m_isModeRTD3GC6)
    {
        entryAction = LW2080_CTRL_GPU_POWER_ON_OFF_MSHYBRID_GC6_ENTER;
        exitAction  = LW2080_CTRL_GPU_POWER_ON_OFF_MSHYBRID_GC6_EXIT;
    }

    for (UINT32 i = 0; i < 2; i++)
    {
        Printf(Tee::PriHigh, "Testing %s S/R path \n", gc6Type.c_str());

        Printf(Tee::PriHigh, "Performing Pre-PowerOff actions\n");
        CHECK_RC(SetGpuPrePostPowerOnOff(LW2080_CTRL_GPU_POWER_PRE_POWER_OFF));

        // Enter RTD3/GC6
        Printf(Tee::PriHigh, "Entering %s \n", gc6Type.c_str());

        Printf(Tee::PriHigh, "calling RM control GC6/RTD3 entry call \n");
        CHECK_RC(SetGpuPowerOnOff(entryAction));

        if (m_isModeRTD3GC6)
        {
            if (m_pSubdevice->IsEmulation())
            {
                if (m_isModeD3Hot)
                {
                    Printf(Tee::PriHigh, "Skip asserting L2 and PexRst for D3Hot\n");
                    Tasker::Sleep(10000);
                }
                else
                {
                    Printf(Tee::PriHigh, "Asserting L2 and PexRst for D3Cold\n");
                    CHECK_RC(m_pSubdevice->SetBackdoorGpuLinkL2(1));
                    // assert pexrst#
                    Tasker::Sleep(10000);
                    CHECK_RC(m_pSubdevice->SetBackdoorGpuAssertPexRst(1));
                }
            }
            else // fmodel
            {
                //
                // The delay value was added based on experiment.
                // If Sleep(20000), I see assert message from fmodel.
                // In production, the value will be queried from VBIOS.
                // See http://lwbugs/200366139/9
                //
                Tasker::Sleep(30000);
                Printf(Tee::PriHigh, "Asserting PCH PEXRST# after predefined delay\n");
                Platform::EscapeWrite("gc6plus_pex_reset",0,2,0x0);
                Platform::EscapeRead("gc6plus_pex_reset",0,2,&gpio);
                Printf(Tee::PriHigh, "PEXRST# signal is asserted.(0x%x)\n", gpio);
            }
        }

        Printf(Tee::PriHigh, "%s entered\n", gc6Type.c_str());

        Tasker::Sleep(10000);

        // Exit RTD3/GC6
        Printf(Tee::PriHigh, "Exiting %s \n", gc6Type.c_str());

        if (m_isModeRTD3GC6)
        {
            if (m_pSubdevice->IsEmulation())
            {
                if (m_isModeD3Hot)
                {
                    Printf(Tee::PriHigh, "Skip de-asserting L2 and PexRst for D3Hot\n");
                }
                else
                {
                    Printf(Tee::PriHigh, "De-asserting L2 and PexRst for D3Cold\n");
                    CHECK_RC(m_pSubdevice->SetBackdoorGpuAssertPexRst(0));
                    CHECK_RC(m_pSubdevice->SetBackdoorGpuLinkL2(0));
                }
            }
            else
            {
                Printf(Tee::PriHigh, "de-asserting PEXRST# \n");
                Platform::EscapeWrite("gc6plus_pex_reset",0,2,0x1);
                Platform::EscapeRead("gc6plus_pex_reset",0,2,&gpio);
                Printf(Tee::PriHigh, "PEXRST# signal is de-asserted.(0x%x)\n", gpio);
            }
        }

        Printf(Tee::PriHigh, "calling RM control GC6/RTD3 exit call \n");
        CHECK_RC(SetGpuPowerOnOff(exitAction));

        Printf(Tee::PriHigh, "Performing Post-PowerOn actions\n");
        CHECK_RC(SetGpuPrePostPowerOnOff(LW2080_CTRL_GPU_POWER_POST_POWER_ON));
    }

    return rc;
}

//! \brief This function runs the test.
//!
//! This calls the function quick Suspend/Resume of GPU for specified number of
//! iteration.
//!
//! \return OK if successful
//!

RC QuickSuspendResumeGPU::Run()
{
    RC rc;
    UINT32 i ;
    m_pwrUtil.BindGpuSubdevice(GetBoundGpuSubdevice());
    AcrVerify acrVerify;

    CHECK_RC(acrVerify.BindTestDevice(GetBoundTestDevice()));

    // call for quick suspend resume with some delay in suspend resume
    if (!m_isModeGC6 && !m_isModeRTD3GC6 && !m_isModeFGC6)
    {
        for (i = 1; i <= m_QsrIterations; i++)
        {
            Printf(Tee::PriHigh, "Performing Pre-PowerOff actions\n");
            CHECK_RC(SetGpuPrePostPowerOnOff(LW2080_CTRL_GPU_POWER_PRE_POWER_OFF));

            rc = m_pwrUtil.RmSetPowerState(m_pwrUtil.GPU_PWR_SUSPEND);
            if (rc != OK)
            {
                Printf(Tee::PriHigh,
                  "%d: Quick Suspend of GPU failed at %d iteration \n",
                      __LINE__, i);
                CHECK_RC(rc);
            }
            else
            {
                Printf(Tee::PriLow,
                   "%d: Quick Suspend of GPU passed for %d iteration \n",
                      __LINE__, i);
            }

            if (IsClassSupported(GF100_CHANNEL_GPFIFO) &&
                !GetBoundGpuSubdevice()->HasBug(739483))
            {
                CHECK_RC(m_pwrUtil.RmSetPowerState(m_pwrUtil.GPU_PWR_RESET));
            }
            else
            {
                // Now dirty the FB since we can't reset
                 rc = m_pwrUtil.RmSetPowerState(m_pwrUtil.GPU_PWR_FB_DIRTY_PARTIAL);
                if (rc != OK)
                {
                    Printf(Tee::PriHigh,
                        "%d: FB taint of GPU failed at %d iteration \n",
                        __LINE__, i);
                    CHECK_RC(rc);
                }
                else
                {
                    Printf(Tee::PriLow,
                    "%d: FB taint of GPU passed for %d iteration \n",
                    __LINE__, i);
                }
            }

            // Resume the gpu
            rc = m_pwrUtil.RmSetPowerState(m_pwrUtil.GPU_PWR_RESUME);
            if (rc != OK)
            {
                Printf(Tee::PriHigh,
                  "%d: Quick Resume Reset of GPU failed at %d iteration \n",
                      __LINE__, i);
                CHECK_RC(rc);
            }
            else
            {
                Printf(Tee::PriLow,
                   "%d: Quick Resume of GPU passed for %d iteration \n",
                      __LINE__, i);
            }

            Printf(Tee::PriHigh, "Performing Post-PowerOn actions\n");
            CHECK_RC(SetGpuPrePostPowerOnOff(LW2080_CTRL_GPU_POWER_POST_POWER_ON));
        }
    }
    else
    {
        DisplayIDs Detected;
        DisplayIDs Supported;
        UINT32 Width,Height,Depth,RefreshRate;
        vector<Surface2D> CoreImage(1);
        vector<ImageUtils> imgArr(1);
        vector<Surface2D> LwrsImage(1);
        vector<ImageUtils> imgArrLwrs(1);

        if (m_TestDisplay)
        {
            CHECK_RC(m_pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

            CHECK_RC(m_pDisplay->GetConnectors(&Supported));
            Printf(Tee::PriHigh, "All supported displays = \n");
            m_pDisplay->PrintDisplayIds(Tee::PriHigh, Supported);

            CHECK_RC(m_pDisplay->GetDetected(&Detected));
            Printf(Tee::PriHigh, "Detected displays  = \n");
            m_pDisplay->PrintDisplayIds(Tee::PriHigh, Detected);

            Width    = 640;
            Height   = 480;
            Depth    = 32;
            RefreshRate  = 60;

            // Set up a core channel image.
            imgArr[0] = ImageUtils::SelectImage(Width, Height);
            CHECK_RC(m_pDisplay->SetupChannelImage(Detected[0],
                                                   Width,
                                                   Height,
                                                   Depth,
                                                   Display::CORE_CHANNEL_ID,
                                                   &CoreImage[0],
                                                   imgArr[0].GetImageName(),
                                                   0,
                                                   0)); //Ask RM Config to allocate
            rc = m_pDisplay->SetMode(Detected[0],Width, Height, Depth, RefreshRate);

            // Set up a cursor image.
            imgArrLwrs[0] = ImageUtils::SelectImage(32, 32);
            CHECK_RC(m_pDisplay->SetupChannelImage(Detected[0],
                                                   32,
                                                   32,
                                                   Depth,
                                                   Display::LWRSOR_IMM_CHANNEL_ID,
                                                   &LwrsImage[0],
                                                   imgArrLwrs[0].GetImageName(),
                                                   0,
                                                   0)); //Ask RM Config to allocate

            rc = m_pDisplay->SetImage(Detected[0], &LwrsImage[0], Display::LWRSOR_IMM_CHANNEL_ID);

        }

        LwRmPtr      pLwRm;
        LW2080_CTRL_POWER_FEATURES_SUPPORTED_PARAMS powerFeaturesSupportedParams = {0};

        // Check support for GC6
        rc = pLwRm->ControlBySubdevice(GetBoundGpuSubdevice(),
                                         LW2080_CTRL_CMD_POWER_FEATURES_SUPPORTED,
                                         &powerFeaturesSupportedParams,
                                         sizeof(powerFeaturesSupportedParams));

        LW2080_CTRL_CMD_GET_RTD3_INFO_PARAMS rtd3InfoParams = {0};

        // Check support for IFR/BSI driven support
        rc = pLwRm->ControlBySubdevice(GetBoundGpuSubdevice(),
                                         LW2080_CTRL_CMD_GET_RTD3_INFO,
                                         &rtd3InfoParams,
                                         sizeof(rtd3InfoParams));

        m_isGC6Supported     = FLD_TEST_DRF(2080_CTRL, _POWER_FEATURES_SUPPORTED, _GC6, _TRUE,
                                   powerFeaturesSupportedParams.powerFeaturesSupported);
        m_isRTD3GC6Supported = FLD_TEST_DRF(2080_CTRL, _POWER_FEATURES_SUPPORTED, _GC6_RTD3, _TRUE,
                                   powerFeaturesSupportedParams.powerFeaturesSupported);
        m_isFGC6Supported    = FLD_TEST_DRF(2080_CTRL, _POWER_FEATURES_SUPPORTED, _FGC6, _TRUE,
                                   powerFeaturesSupportedParams.powerFeaturesSupported);

        if (!m_isGC6Supported && !m_isRTD3GC6Supported)
        {
            Printf(Tee::PriHigh, "Both GC6/RTD3 not supported \n");
            return RC::SOFTWARE_ERROR;
        }

        if (m_isFGC6Supported && !m_isRTD3GC6Supported)
        {
            Printf(Tee::PriHigh, "FGC6 only supported with RTD3 \n");
            return RC::SOFTWARE_ERROR;
        }

        //
        // If it's BSI driven path, wait for RAM_VALID (SCI/BSI to load)
        // If it's IFR driven path, we don't need to wait island load as
        // BSIRAM won't be programmed
        //
        if (!rtd3InfoParams.bIsRTD3IFRPathSupported)
        {
            CHECK_RC(WaitForGC6PlusIslandLoad());
        }

        CHECK_RC(TestSuspendResume());

        if (m_TestDisplay)
        {
            CHECK_RC(m_pDisplay->DetachDisplay(DisplayIDs(1, Detected[0])));

            if(CoreImage[0].GetMemHandle() != 0)
                CoreImage[0].Free();

            if(LwrsImage[0].GetMemHandle() != 0)
                LwrsImage[0].Free();
        }
    }

    // Verify ACR and LS falcon setups
    if( acrVerify.IsTestSupported() == RUN_RMTEST_TRUE )
    {
        CHECK_RC(acrVerify.Setup());
        // Run Verifies ACM setup and status of each Falcon.
        rc = acrVerify.Run();
    }

    return rc;
}

RC QuickSuspendResumeGPU::SetGpuPrePostPowerOnOff(UINT32 action)
{
    RC       rc = OK;
    LwRmPtr  pLwRm;
    LW2080_CTRL_GPU_POWER_PRE_POST_POWER_ON_OFF_PARAMS params = {0};

    params.action = action;
    rc = pLwRm->ControlBySubdevice(GetBoundGpuSubdevice(),
                                   LW2080_CTRL_CMD_GPU_POWER_PRE_POST_POWER_ON_OFF,
                                   &params,
                                   sizeof(params));
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
              "%d: SetGpuPrePostPowerOnOff failed, action: %d \n", __LINE__, action);
        CHECK_RC(rc);
    }
    return rc;
}

RC QuickSuspendResumeGPU::SetGpuPowerOnOff(UINT32 action)
{
    RC           rc;
    LwRmPtr      pLwRm;
    LW2080_CTRL_GC6_ENTRY_PARAMS entryParams = {};
    LW2080_CTRL_GC6_EXIT_PARAMS exitParams = {};

    // action == enter
    if (action == LW2080_CTRL_GPU_POWER_ON_OFF_GC6_ENTER ||
        action == LW2080_CTRL_GPU_POWER_ON_OFF_FAST_GC6_ENTER ||
        action == LW2080_CTRL_GPU_POWER_ON_OFF_MSHYBRID_GC6_ENTER)
    {
        entryParams.stepMask = BIT(LW2080_CTRL_GC6_STEP_ID_GPU_OFF);

        //
        // If the action is to enter GC6, program the wakeup timer as well
        // Program SCI wakeup timer to 100 us. Make this a large enough number
        // so that emulation and fmodel can use the same sleep time.
        // RTD3 do not use HW timer to wake-up but PEXRST# signal instead.
        // FGC6 uses GC6 method of wakeup.
        //
        if (action == LW2080_CTRL_GPU_POWER_ON_OFF_GC6_ENTER ||
            action == LW2080_CTRL_GPU_POWER_ON_OFF_FAST_GC6_ENTER)
        {
            entryParams.flavorId = LW2080_CTRL_GC6_FLAVOR_ID_OPTIMUS;
            if (m_isModeGC6 || m_isModeFGC6)
            {
                entryParams.params.bUseHwTimer   = 1;
                entryParams.params.timeToWakeUs  = 100;
            }
            else
            {
                //
                // Normal suspend/resume is in another path.
                // SetGpuPowerOnOff() is calling RmCtrl call and is dedicated for GC6/RTD3
                // Given m_isModeGC6 and m_isModeRTD3GC6 is from cmd line arguments,
                // this is a stub path. Will never be here
                //
                Printf(Tee::PriHigh, "This is an error that GC6/RTD3/FGC6 not supported \n");
                return RC::SOFTWARE_ERROR;
            }
        }
        else if (action == LW2080_CTRL_GPU_POWER_ON_OFF_MSHYBRID_GC6_ENTER)
        {
            entryParams.flavorId = LW2080_CTRL_GC6_FLAVOR_ID_MSHYBRID;
            if (m_isModeRTD3GC6)
            {
                entryParams.params.bIsRTD3Transition = LW_TRUE;
            }
        }

        rc = pLwRm->ControlBySubdevice(GetBoundGpuSubdevice(),
                               LW2080_CTRL_CMD_GC6_ENTRY,
                               &entryParams,
                               sizeof(entryParams));
    }
    // action == exit
    else if (action == LW2080_CTRL_GPU_POWER_ON_OFF_GC6_EXIT ||
             action == LW2080_CTRL_GPU_POWER_ON_OFF_FAST_GC6_EXIT ||
             action == LW2080_CTRL_GPU_POWER_ON_OFF_MSHYBRID_GC6_EXIT)
    {
        if (action == LW2080_CTRL_GPU_POWER_ON_OFF_GC6_EXIT ||
            action == LW2080_CTRL_GPU_POWER_ON_OFF_FAST_GC6_EXIT)
        {
            exitParams.flavorId = LW2080_CTRL_GC6_FLAVOR_ID_OPTIMUS;
        }
        else if (action == LW2080_CTRL_GPU_POWER_ON_OFF_MSHYBRID_GC6_EXIT)
        {
            exitParams.flavorId = LW2080_CTRL_GC6_FLAVOR_ID_MSHYBRID;
        }

        rc = pLwRm->ControlBySubdevice(GetBoundGpuSubdevice(),
                               LW2080_CTRL_CMD_GC6_EXIT,
                               &exitParams,
                               sizeof(exitParams));
    }

    if (rc != OK)
    {
        Printf(Tee::PriHigh,
              "%d: SetGpuPowerOnOff failed, action: %d \n", __LINE__, action);
        CHECK_RC(rc);
    }
    return rc;
}

//!
//!
//! \brief Wait for SCI/BSI to load.
//!
//! \return RC
RC QuickSuspendResumeGPU::WaitForGC6PlusIslandLoad()
{
    RC           rc;
    LwRmPtr      pLwRm;
    LW2080_CTRL_GC6PLUS_IS_ISLAND_LOADED_PARAMS params;

    Printf(Tee::PriHigh,"GC6 test : Waiting for SCI/BSI to be loaded\n");

    while (1)
    {
        memset(&params, 0, sizeof(params));

        rc = pLwRm->ControlBySubdevice(GetBoundGpuSubdevice(),
                                       LW2080_CTRL_CMD_GC6PLUS_IS_ISLAND_LOADED,
                                       &params,
                                       sizeof(params));

        if (params.bIsIslandLoaded != LW_FALSE)
            break;

        Tasker::Sleep(1000);
    }

    Printf(Tee::PriHigh,"GC6 test : Island is loaded. Free to Enter GC6 now.\n");
    return rc;
}

//------------------------------------------------------------------------------

//! \brief Cleanup()
//!
//! free the resources allocated
//!
//! \sa Setup
//------------------------------------------------------------------------------
RC QuickSuspendResumeGPU::Cleanup()
{
    RC rc;
    LwRmPtr pLwRm;

    // Restore pstate to before the test
    if (!m_NoForceP8 &&
        (m_isModeGC6 || m_isModeRTD3GC6 || m_isModeFGC6))
    {
        LW2080_CTRL_PERF_SET_FORCE_PSTATE_PARAMS setForcePStateParams;

        Printf(Tee::PriHigh, "Restoring pstate to 0x%x\n", m_initPState);

        MASSERT(m_initPState <= LW2080_CTRL_PERF_PSTATES_MAX &&
                m_initPState >= LW2080_CTRL_PERF_PSTATES_P0);

        // Switch to original pstate from before the test
        setForcePStateParams.forcePstate = m_initPState;
        setForcePStateParams.fallback = LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR;

        CHECK_RC(pLwRm->ControlBySubdevice(m_pSubdevice,
                 LW2080_CTRL_CMD_PERF_SET_FORCE_PSTATE,
                 &setForcePStateParams, sizeof(setForcePStateParams)));
    }

    return OK;
}

// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++
//! QuickSuspendResumeGPU object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(QuickSuspendResumeGPU, RmTest,
    "Test used for quick Suspend/Resume of GPU");
CLASS_PROP_READWRITE(QuickSuspendResumeGPU, QsrIterations, UINT32,
    "No. of iterations for running Suspend-Resume.");
CLASS_PROP_READWRITE(QuickSuspendResumeGPU, isModeGC6, bool,
    "Test GC6 S/R path, default = false");
CLASS_PROP_READWRITE(QuickSuspendResumeGPU, isModeRTD3GC6, bool,
    "Test RTD3 S/R path, default = false");
CLASS_PROP_READWRITE(QuickSuspendResumeGPU, isModeD3Hot, bool,
    "Test RTD3 D3Hot S/R path, must used with isModeRTD3GC6 and RM regkey RMForceRtd3D3Hot 0x1, default = false");
CLASS_PROP_READWRITE(QuickSuspendResumeGPU, isModeFGC6, bool,
    "Test FGC6 S/R path, default = false");
CLASS_PROP_READWRITE(QuickSuspendResumeGPU, TestDisplay, bool,
    "Test GC6 with display S/R path, default = false");
CLASS_PROP_READWRITE(QuickSuspendResumeGPU, NoForceP8, bool,
    "Do not force test switch to P8 when testing");
