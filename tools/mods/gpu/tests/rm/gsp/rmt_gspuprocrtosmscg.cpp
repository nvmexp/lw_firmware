/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_gspuprocrtosmscg.cpp
//! \brief rmtest for GSP
//!

#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/tests/rm/utility/uprocrtos.h"
#include "gpu/tests/rm/utility/uprocgsprtos.h"
#include "gpu/tests/rm/memsys/rmt_acrverify.h"
#include "rmlsfm.h"
#include "core/include/platform.h"
#include "core/include/lwrm.h"
#include "gpu/include/gpusbdev.h"
#include "core/utility/errloggr.h"
#include "core/include/tasker.h"
#include "uprocifcmn.h"

class GspUprocRtosMscgTest : public RmTest
{
public:
    GspUprocRtosMscgTest();
    virtual ~GspUprocRtosMscgTest()
    {
    };
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(LoopVar, UINT32);
    SETGET_PROP(wakeupMethod, UINT32);

private:
    RC  checkGspRtosBootMode();

    unique_ptr<UprocRtos> m_pGspRtos;
    unsigned int    m_Platform;
    LwU32           m_wakeupMethod;
    FLOAT64         m_timeOutMs;
    FLOAT64         m_waitTimeMs;

    // Holds the number of loops for running the test.
    LwU32           m_LoopVar;
};

//! \brief GspMscgTest constructor
//!
//! Doesn't do much except SetName.
//!
//! \sa Setup
//------------------------------------------------------------------------------
GspUprocRtosMscgTest::GspUprocRtosMscgTest()
{
    SetName("GspRtosMscgTest");
    m_timeOutMs = 1000.0f;
    m_waitTimeMs = 1000.0f;
    m_Platform = Platform::Hardware;
    m_LoopVar = 0;
    m_wakeupMethod = UPROCRTOS_UPROC_REG_ACCESS;
}

//! \brief IsTestSupported
//!
//! 1. Checks the family of the GPU (Kepler, maxwell, pascal)
//! 2. Checks the platform for current testing (Fmodel, silicon, amodel, RTL)
//! 3. Checks the support status of the feature (MSCG supported)
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string GspUprocRtosMscgTest::IsTestSupported()
{
    RC rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_POWERGATING_PARAMETER PgParam = {0};
    LW2080_CTRL_MC_QUERY_POWERGATING_PARAMETER_PARAMS  QueryParams = {0};
    GpuSubdevice *subDev = GetBoundGpuSubdevice();

    // Detect if the GPU is from a supported family.
    Gpu::LwDeviceId chipName = GetBoundGpuSubdevice()->DeviceId();
    if (chipName < Gpu::GV100)
    {
        return "GSP not supported.\n";
    }

    const RegHal &regs  = subDev->Regs();
    UINT32  regVal      = regs.Read32(MODS_PGSP_SCP_CTL_STAT);
    LwBool  bProdBoard  = regs.TestField(regVal, MODS_PGSP_SCP_CTL_STAT_DEBUG_MODE_DISABLED);

    if (bProdBoard)
    {
        return "\nGSP RTOS : Not supported in prod Mode.\n";
    }

    // Get the current Platform for the simulation (HW/FMODEL/RTL). Other Platforms are not supported.
    m_Platform = Platform::GetSimulationMode();
    if (m_Platform == Platform::Fmodel)
        Printf(Tee::PriHigh, "The Platform is FModel.\n");
    else if (m_Platform == Platform::Hardware)
        Printf(Tee::PriHigh, "The Platform is Hardware.\n");
    else if (m_Platform == Platform::RTL)
        Printf(Tee::PriHigh, "The Platform is RTL.\n");
    else
        return "Unknown/Unsupported Platform found.\n";

    // Check if the MSCG is enabled on the GPU.
    PgParam.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_SUPPORTED;
    PgParam.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;
    QueryParams.listSize      = 1;
    QueryParams.parameterList = (LwP64) &PgParam;
    rc = pLwRm->ControlBySubdevice(subDev,
                                   LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER,
                                   &QueryParams,
                                   sizeof(QueryParams)
                                  );
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "\n %s: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER Query failed with rc=%d\n", __FUNCTION__,(UINT32)rc);
    }
    if (PgParam.parameterValue == 0)
    {
        return "MSCG not enabled on the GPU.\n";
    }

    m_pGspRtos = std::make_unique<UprocGspRtos>(subDev);
    if (m_pGspRtos == nullptr)
    {
        return "Could not create UprocGspRtos instance.\n";
    }
    rc = m_pGspRtos->Initialize();
    if (OK != rc)
    {
        m_pGspRtos->Shutdown();
        return "[GSPUproc MSCG Test] : GSP RTOS not supported\n";
    }

    if (m_wakeupMethod == UPROCRTOS_UPROC_PB_ACTIVITY)
    {
        return "[GSPUproc MSCG Test] : PB_ACTIVITY wakeup method not supported on GSP\n";
    }
    if (m_wakeupMethod == UPROCRTOS_UPROC_REG_ACCESS &&
        !m_pGspRtos->CheckTestSupported(RM_UPROC_CMD_TYPE(TEST, RD_BLACKLISTED_REG)))
    {
        return "[GSPUproc MSCG Test] CheckTestSupported failed: Ucode does not support RD_BLACKLISTED_REG\n";
    }
    if (m_wakeupMethod == UPROCRTOS_UPROC_FB_ACCESS &&
        !m_pGspRtos->CheckTestSupported(RM_UPROC_CMD_TYPE(TEST, MSCG_ISSUE_FB_ACCESS)))
    {
        return "[GSPUproc MSCG Test] CheckTestSupported failed: Ucode does not support MSCG_ISSUE_FB_ACCESS\n";
    }

    if (m_LoopVar <= 1)
    {
        return "[GSPUproc MSCG Test] At least two loops must be selected to run MSCG test.\n";
    }

    return RUN_RMTEST_TRUE;
}

//! \brief Do Init from Js
//!
//! \return OK if no error happens, otherwise corresponding rc
//------------------------------------------------------------------------------
RC GspUprocRtosMscgTest::Setup()
{
    RC rc;
    ErrorLogger::StartingTest();
    ErrorLogger::IgnoreErrorsForThisTest();

    // Initialize RmTest handle
    m_pGspRtos->SetRmTestHandle(this);

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    // The Test will wait for g_GspLpwrWaitTimeMs time for allowing the feature to allow the gating of the feature
    m_timeOutMs = GetTestConfiguration()->TimeoutMs();

    switch (m_Platform)
    {
        case Platform::Fmodel:
            // increase the time out on fmodel and base
            // it as a multiple of TimeoutMs
            m_waitTimeMs = m_timeOutMs * 10;
            break;
        case Platform::Hardware:
            m_waitTimeMs = m_timeOutMs;
            break;
        default:
            // increasing the time out on RTL. This time-out period is not tested yet.
            m_waitTimeMs = m_timeOutMs * 100;
            break;
    };

    Printf(Tee::PriHigh, "\nThe number of loops selected is : %u\n", m_LoopVar);

    switch (m_wakeupMethod)
    {
        case UPROCRTOS_UPROC_REG_ACCESS:
        {
            Printf(Tee::PriHigh, "The GSP - Register Access mechanism is used for the test.\n");
            CHECK_RC(checkGspRtosBootMode());
            break;
        }
        case UPROCRTOS_UPROC_FB_ACCESS:
        {
            Printf(Tee::PriHigh, "The GSP - Framebuffer Access mechanism is used for the test.\n");
            CHECK_RC(checkGspRtosBootMode());
            CHECK_RC(m_pGspRtos->SetupFbSurface(MSCG_ISSUE_FB_SIZE));
            break;
        }
        default:
        {
            Printf(Tee::PriHigh, "Error: Unknown Wakeup method is selected!");
            return RC::SOFTWARE_ERROR;
            break;
        }
    }
    Printf(Tee::PriHigh, "The Time-Out period is :%f\n", m_waitTimeMs);

    m_TestConfig.SetChannelType(TestConfiguration::GpFifoChannel);
    return OK;
}

// Make sure GSP RTOS is booted in the expected mode.
//
RC GspUprocRtosMscgTest::checkGspRtosBootMode()
{
    RC rc;
#if GSPUPROCRTOS_USE_ACR
    // Please refer to uprocgsprtos.h for why ACR is disabled for GSP tests.
    AcrVerify lsGspState;
#endif

    // Waiting for GSP to bootstrap.
    rc = m_pGspRtos->WaitUprocReady();
    if(OK != rc)
    {
        Printf(Tee::PriHigh, "Ucode OS for GSP is not ready. Skipping test.\n");
        // fail the test, we need to figure out why GSP bootstrap failed.
        CHECK_RC(RC::LWRM_STATUS_ERROR_NOT_READY);
        return rc;
    }

#if GSPUPROCRTOS_USE_ACR
    if(lsGspState.IsTestSupported() == RUN_RMTEST_TRUE)
    {
        lsGspState.Setup();
        // Verify is GSP is booted in LS mode.
        rc = lsGspState.VerifyFalconStatus(LSF_FALCON_ID_DPU, LW_TRUE);
        if(OK != rc)
        {
            Printf(Tee::PriHigh, "GSP failed to boot in expected Mode.\n");
            return rc;
        }
    }
    else
    {
        Printf(Tee::PriHigh, "Falcon test is not supported on the config.\n");
        return RC::SOFTWARE_ERROR;
    }
#else
    Printf(Tee::PriHigh, "AcrVerify skipped!\n");
#endif

    Printf(Tee::PriHigh, "The GSP Uproc has been setup successfully.\n");
    return rc;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//! \brief Run the test
//!
//! Make sure GSP is bootstrapped.  Then run test items.
//!
//! \return OK all tests pass, else corresponding rc
//------------------------------------------------------------------------------
RC GspUprocRtosMscgTest::Run()
{
    RC rc = OK;
    CHECK_RC(m_pGspRtos->UprocRtosRunTests(UPROC_TEST_TYPE_MSCG, m_wakeupMethod, m_LoopVar, m_timeOutMs, m_waitTimeMs));
    return rc;
}

//! \brief Cleanup, frees allocated resources and shuts down GSP test instance.
//------------------------------------------------------------------------------
RC GspUprocRtosMscgTest::Cleanup()
{
    RC rc;
    CHECK_RC(m_pGspRtos->FreeResources());
    m_pGspRtos->Shutdown();
    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(GspUprocRtosMscgTest, RmTest,
    "GSPUproc RTOS test for MSCG verification.");

CLASS_PROP_READWRITE(GspUprocRtosMscgTest, LoopVar,             LwU32, "Loops for all Functional Tests.");
CLASS_PROP_READWRITE(GspUprocRtosMscgTest, wakeupMethod,        LwU32, "Select the Wakeup Method to be used.");
