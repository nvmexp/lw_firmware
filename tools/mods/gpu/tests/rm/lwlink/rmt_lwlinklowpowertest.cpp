/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \rmt_lwlinklowpower.cpp
//! \Performs lwlink low power state transitions
//!
#include "gpu/tests/rmtest.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gpumgr.h"

#include "core/include/jscript.h"
#include "core/include/mgrmgr.h"
#include "core/include/platform.h"

#include "ctrl/ctrl2080.h"

#include "rmt_lwlinkverify.h"

// Maximum number of iterations for which the test runs
#define LWLINK_LOWPOWER_MAX_ITERATIONS     3

class LwlinkLowPowerTest : public RmTest
{
    GpuDevMgr    *m_pGpuDevMgr = nullptr;
    UINT32        m_NumDevices = 0;
    LwlinkVerify  lwlinkVerif;

    UINT32        GetNumSubDevices();

public:

    LwlinkLowPowerTest();
    virtual ~LwlinkLowPowerTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
};

//!
//! \brief LwlinkLowPowerTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
LwlinkLowPowerTest::LwlinkLowPowerTest()
{
    SetName("LwlinkLowPowerTest");
}

//!
//! \brief LwlinkLowPowerTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
LwlinkLowPowerTest::~LwlinkLowPowerTest()
{
   // Taken care in CleanUp.
}

//!
//! \brief Whether or not the test is supported in the current environment
//!
//! On the current environment where the test is been exelwted there are
//! chances that classes that are required by this test are not supported.
//! IsTestSupported() checks to see whether those classes are supported or
//! not and this decides whether the current test should be ilwoked or not.
//! If supported then the test will be ilwoked.
//!
//! \return True if the required classes are supported in the current
//!         environment, false otherwise
//-----------------------------------------------------------------------------
string LwlinkLowPowerTest::IsTestSupported()
{
    GpuDevice    *pDev    = NULL;
    GpuSubdevice *pSubdev = NULL;

    UINT32 i = 0;

    // Get the number of subdevices on the system
    m_pGpuDevMgr = (GpuDevMgr*)DevMgrMgr::d_GraphDevMgr;
    m_NumDevices = GetNumSubDevices();

    do
    {
        // Get the devices from the device manager
        pDev = (i == 0 ? m_pGpuDevMgr->GetFirstGpuDevice() :
                         m_pGpuDevMgr->GetNextGpuDevice(pDev));

        // Get the subdevice for the device
        pSubdev = pDev->GetSubdevice(0);

        // Check if the subdevice supports LWLink L2
        if (!lwlinkVerif.isPowerStateSupported(pSubdev, LW2080_CTRL_LWLINK_POWER_STATE_L0) ||
            !lwlinkVerif.isPowerStateSupported(pSubdev, LW2080_CTRL_LWLINK_POWER_STATE_L2))
        {
            return "GPU does not support LWLink L2";
        }

        // Check if the links on the subdevice have the power state enabled
        if (!lwlinkVerif.isPowerStateEnabled(pSubdev, LW2080_CTRL_LWLINK_POWER_STATE_L0) ||
            !lwlinkVerif.isPowerStateEnabled(pSubdev, LW2080_CTRL_LWLINK_POWER_STATE_L2))
        {
            return "LWLink L2 is disabled on the GPU";
        }

        i++;
    } while (i < m_NumDevices);

    return RUN_RMTEST_TRUE;
}

//!
//! \brief Setup all necessary state before running the test.
//!
//! Idea is to reserve all the resources required by this test so
//! if required multiple tests can be ran in parallel.
//! \return RC -> OK if everything is allocated, test-specific RC
//!         if something failed while selwring resources.
//! \sa IsTestSupported
//-----------------------------------------------------------------------------
RC LwlinkLowPowerTest::Setup()
{
    // Setup the LWLink connections for the subdevices
    lwlinkVerif.Setup(m_pGpuDevMgr, m_NumDevices);

    return OK;
}

//!
//! \brief Run the test!
//!
//! \return OK if the test passed, specific RC if it failed
//-----------------------------------------------------------------------------
RC LwlinkLowPowerTest::Run()
{
    UINT32 i;

    // If there are no links available, print and return
    if (lwlinkVerif.getTotalLinksFound() == 0)
    {
        Printf(Tee::PriHigh, "No LWLink connections found ! Returning\n");
        return OK;
    }

    for (i = 0; i < LWLINK_LOWPOWER_MAX_ITERATIONS; i++)
    {
        Printf(Tee::PriHigh, "Loop %d: Entering LWLink L2 on all connections\n", i);

        // Transition the links to L2 state
        if (!lwlinkVerif.verifyPowerStateTransition(LW2080_CTRL_LWLINK_POWER_STATE_L2))
        {
            return RC::LWLINK_BUS_ERROR;
        }

        Printf(Tee::PriHigh, "Loop %d: Exiting LWLink L2 on all connections\n", i);

        // Transition the links to L0 state
        if (!lwlinkVerif.verifyPowerStateTransition(LW2080_CTRL_LWLINK_POWER_STATE_L0))
        {
            return RC::LWLINK_BUS_ERROR;
        }

        Printf(Tee::PriHigh, "Loop %d: LWLink L2 passed on all connections\n", i);
    }

    return OK;
}

//!
//! \brief LwlinkLowPowerTest Cleanup.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC LwlinkLowPowerTest::Cleanup()
{
    lwlinkVerif.Cleanup();

    return OK;
}

//!
//! Get the number of devices in the machine.
//!----------------------------------------------------------------------------
UINT32 LwlinkLowPowerTest::GetNumSubDevices()
{
    LwRmPtr pLwRm;
    UINT32  NumSubdevices = 0;

    LW0000_CTRL_GPU_GET_ATTACHED_IDS_PARAMS gpuAttachedIdsParams = {{0}};

    pLwRm->Control(pLwRm->GetClientHandle(),
                   LW0000_CTRL_CMD_GPU_GET_ATTACHED_IDS,
                   &gpuAttachedIdsParams,
                   sizeof(gpuAttachedIdsParams));

    // Count num devices and subdevices.
    for (UINT32 i = 0; i < LW0000_CTRL_GPU_MAX_ATTACHED_GPUS; i++)
    {
        if (gpuAttachedIdsParams.gpuIds[i] == GpuDevice::ILWALID_DEVICE)
            continue;
        NumSubdevices++;
    }

    return NumSubdevices;
}

//-----------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ LwlinkLowPowerTest object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(LwlinkLowPowerTest, RmTest,
                 "LWLink low power transitions test.");
