/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_GspUprocRtosCommon.cpp
//! \brief rmtest for UPROC GSP
//!

#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/tests/rm/utility/uprocrtos.h"
#include "gpu/tests/rm/utility/uprocgsprtos.h"
#include "gpu/tests/rm/memsys/rmt_acrverify.h"
#include "rmlsfm.h"
#include "class/cl9070.h"
#include "class/cl9270.h"
#include "class/cl9470.h"
#include "class/cl9570.h"
#include "class/clc310.h"
#include "ctrl/ctrlc310.h"

#define SET_YELLOW      printf("\033[1;33m");
#define RESET_COLOR     printf("\033[0m");

/*
* N.B. List of sub tasks for now
*   RM_UPROC_COMMON_TEST_MEM_OPS                     <Enabled>
*   RM_UPROC_COMMON_TEST_DMA                         <Enabled>
*   RM_UPROC_COMMON_TEST_KERNEL_CRITICAL_SECTIONS    <Failling>
*   RM_UPROC_COMMON_TEST_KERNEL_MODE                 <Enabled>
*   RM_UPROC_COMMON_TEST_TIMER_CALLBACK              <Not Implemented>
*   RM_UPROC_COMMON_TEST_UMODE_CTX_SWITCH            <Not Implemented>
*   RM_UPROC_COMMON_TEST_KMODE_CTX_SWITCH            <Not Implemented>
*   RM_UPROC_COMMON_TEST_MAX                         <Not Implemented>
*/

// Combined Mask of all Sub-Tests/Tasks Disabled for now.
#define DISABLE_MASK  ((1 <<  RM_UPROC_COMMON_TEST_KERNEL_CRITICAL_SECTIONS)|       \
                       (1 << RM_UPROC_COMMON_TEST_TIMER_CALLBACK)           |       \
                       (1 << RM_UPROC_COMMON_TEST_UMODE_CTX_SWITCH)         |       \
                       (1 << RM_UPROC_COMMON_TEST_KMODE_CTX_SWITCH))

class GspUprocRtosCommonTest : public RmTest
{
public:
    GspUprocRtosCommonTest();
    virtual ~GspUprocRtosCommonTest()
    {
    };
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(Subtstmask, UINT32);

private:
    unique_ptr<UprocRtos>       m_pGspRtos;
    UINT32                      m_Subtstmask;
};

//! \brief GspUprocRtosCommonTest constructor
//!
//! Doesn't do much except SetName.
//!
//! \sa Setup
//------------------------------------------------------------------------------
GspUprocRtosCommonTest::GspUprocRtosCommonTest()
{
    SetName("GspRtosCommonTest");
    m_Subtstmask = 0;
}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string GspUprocRtosCommonTest::IsTestSupported()
{
    LwRmPtr      m_pLwRm;

    if (m_pLwRm->IsClassSupported(VOLTA_GSP, GetBoundGpuDevice()) == true )
    {
        return RUN_RMTEST_TRUE;
    }

    return "[GSP UPROCRTOS RMTest] : GSP is not supported on current platform";
}

//! \brief Do Init from Js
//!
//! \return OK if no error happens, otherwise corresponding rc
//------------------------------------------------------------------------------
RC GspUprocRtosCommonTest::Setup()
{
    RC rc;

    // Get GSP UPROCRTOS class instance
    m_pGspRtos = std::make_unique<UprocGspRtos>(GetBoundGpuSubdevice());
    if (m_pGspRtos == nullptr)
    {
        Printf(Tee::PriHigh, "Could not create UprocGspRtos instance.\n");
        return RC::SOFTWARE_ERROR;
    }
    rc = m_pGspRtos->Initialize();
    if (OK != rc)
    {
        m_pGspRtos->Shutdown();
        Printf(Tee::PriHigh, "GSP UPROC RTOS not supported\n");
        CHECK_RC(rc);
    }

    // Initialize RmTest handle
    m_pGspRtos->SetRmTestHandle(this);

    CHECK_RC(InitFromJs());
    return OK;
}

//! \brief Run the test
//!
//! Make sure GSP is bootstrapped.  Then run test items.
//!
//! \return OK all tests pass, else corresponding rc
//------------------------------------------------------------------------------
RC GspUprocRtosCommonTest::Run()
{
    RC rc;
#if GSPUPROCRTOS_USE_ACR
    // Please refer to uprocgsprtos.h for why ACR is disabled for GSP tests.
    AcrVerify lsGspState;
#endif

    rc = m_pGspRtos->WaitUprocReady();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "%d:GspUprocRtosCommonTest::%s: Ucode OS is not ready. Skipping tests. \n",
                __LINE__,__FUNCTION__);

        // fail the test, we need to figure out why GSP bootstrap failed.
        CHECK_RC(RC::LWRM_STATUS_ERROR_NOT_READY);
        return rc;
    }
#if GSPUPROCRTOS_USE_ACR
    if (lsGspState.IsTestSupported() == RUN_RMTEST_TRUE)
    {
        lsGspState.Setup();
        // Verify whether GSP is booted in the expected mode or not
        rc = lsGspState.VerifyFalconStatus(LSF_FALCON_ID_DPU, LW_TRUE); /* TODO: this might not work at all */
        if (rc != OK)
        {
            Printf(Tee::PriHigh, "%d: GspUprocRtosCommonTest %s: GSP failed to boot in expected Mode\n",
                   __LINE__, __FUNCTION__);
            CHECK_RC(RC::SOFTWARE_ERROR);
        }
    }
    else
    {
        Printf(Tee::PriHigh, "%d: GspUprocRtosCommonTest %s: LS Falcon test not supported\n",
               __LINE__, __FUNCTION__);
        CHECK_RC(RC::SOFTWARE_ERROR);
    }
#else
    Printf(Tee::PriHigh,
        "%d:GspUprocRtosCommonTest::%s: AcrVerify skipped! \n",
        __LINE__,__FUNCTION__);
#endif

    if (!m_Subtstmask) {
        Printf(Tee::PriHigh, "No Sub-Test/Task Num Specified. Abort!!!\n");
        return RC::ILWALID_INPUT;
    }

    // Run GSP Common Test Task
    Printf(Tee::PriHigh,
           "%d:GspUprocRtosCommonTest::%s: Starts GSP RTOS Common Test...\n",
            __LINE__,__FUNCTION__);

    for (int i = 1; i < RM_UPROC_COMMON_TEST_MAX; i++) 
    {
        UINT32 combMask = ~DISABLE_MASK & m_Subtstmask;
        if ((1 << i) & combMask)
        {
            SET_YELLOW;
            Printf(Tee::PriHigh, ":=====================>> Testing GSP Common Sub Task %d <<==========================:\n", i);
            RESET_COLOR;
            CHECK_RC(m_pGspRtos->UprocRtosRunTests(UPROC_TEST_TYPE_COMMON, i));
        }
    }

    return rc;
}

//! \brief Cleanup, shuts down GSP test instance.
//------------------------------------------------------------------------------
RC GspUprocRtosCommonTest::Cleanup()
{
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
JS_CLASS_INHERIT(GspUprocRtosCommonTest, RmTest, "GSP UPROCRTOS Common Test. \n");
CLASS_PROP_READWRITE(GspUprocRtosCommonTest, Subtstmask, UINT32, "BitWise Mask representing the SubTests to be Run.Each Set Bit represents the Sub-test Number to be Run");
