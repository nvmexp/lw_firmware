/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2008,2015-2016,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_repeatertest.cpp
//! \brief Test for validating interface for getting HDCP repeater information
//!
//! This specifically tests the new interface added to get the HDCP repeater
//! information. The HDCP repeater information mainly will give the KSV List,
//! VPRIME, MPRIME and Dksv values. The interface also outputs the information
//! whether this is internal HDCP or external one.

#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "lwRmApi.h"
#include "core/include/tasker.h"

#include "class/cl0040.h" // LW01_MEMORY_LOCAL_USER
#include "class/cl0073.h"
#include "class/cl0080.h"
#include "class/cl2080.h"

#include "ctrl/ctrl0000.h"
#include "ctrl/ctrl0073.h"
#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl2080.h"
#include "core/include/memcheck.h"

class HdcpRepeaterTest : public RmTest
{
public:
    HdcpRepeaterTest();
    virtual ~HdcpRepeaterTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    LwRm::Handle m_hDev;
    LwRm::Handle m_hDisplay;
    LwRm::Handle m_hSubDev;

};

//! \brief HdcpRepeaterTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
HdcpRepeaterTest::HdcpRepeaterTest()
{
    SetName("HdcpRepeaterTest");
    m_hDev     = 0;
    m_hDisplay = 0;
    m_hSubDev  = 0;
}

//! \brief HdcpRepeaterTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
HdcpRepeaterTest::~HdcpRepeaterTest()
{
}

//! \brief IsTestSupported:whether this test is supported on current environment
//!
//! This test is supported only on hardware as the HDCP related simulations
//! not available
//!
//! \return True : Can be exelwted on current platform false otherwise
//-----------------------------------------------------------------------------
string HdcpRepeaterTest::IsTestSupported()
{
    if( Platform::GetSimulationMode() == Platform::Hardware )
        return RUN_RMTEST_TRUE;
    return "Supported only on hardware as the HDCP related simulations";
}

//! \brief Setup Function, used for the test related allocations
//!
//! As this test requires the display device allocation for the current GPU so
//! that allocation is done in the setup
//!
//! \return OK if the allocation success, specific RC if it failed.
//! \sa Run
//-----------------------------------------------------------------------------
RC HdcpRepeaterTest::Setup()
{
    RC rc;
    LwRmPtr pLwRm;

    m_hDev = pLwRm->GetDeviceHandle(GetBoundGpuDevice());
    m_hSubDev = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());

    CHECK_RC(InitFromJs());
    CHECK_RC(pLwRm->Alloc(m_hDev, &m_hDisplay, LW04_DISPLAY_COMMON, NULL));
    return rc;
}

//! \brief Run Function.
//!
//! Run function actually calls the RM Control interface to get the HDCP
//! repeater information.
//!
//! \return OK if the tests passed, specific RC if failed
//-----------------------------------------------------------------------------
RC HdcpRepeaterTest::Run()
{
    RC rc;
    LwRmPtr pLwRm;
    LW0073_CTRL_SPECIFIC_GET_HDCP_REPEATER_INFO_PARAMS hdcpInfoStruct = {0};
    LW0073_CTRL_SYSTEM_GET_ACTIVE_PARAMS dispActiveParams = {0};
    LW0073_CTRL_SYSTEM_GET_NUM_HEADS_PARAMS numHeadsParams = {0};
    UINT32 head;

    //
    // Using following variables to do the 5 second timeout
    // pauseVal refers to 500 milliseconds and iterative count
    // is 10 so for each iteration wait for 500 miliseconds making that 5 secs.
    //
    const UINT32 pauseVal = 500;
    const UINT32 iterations = 10;

    const UINT32 arrayCn[LW0073_CTRL_CN_SIZE] = {0x652c2f27, 0x2c72677f};
    const UINT08 arrayCksv[LW0073_CTRL_CKSV_SIZE] =
                                        {0x8c, 0xfc, 0x46, 0x66, 0xb8};

    // For Display mask
    INT32 status = 0;
    UINT32 deviceCount = 0;
    UINT08 iLoopVar;
    UINT32 itempVar;

    // Get Display mask information using default method and default subdevice

    // get number of heads
    numHeadsParams.subDeviceInstance = 0;
    numHeadsParams.flags = 0;
    CHECK_RC(pLwRm->Control(m_hDisplay,
                         LW0073_CTRL_CMD_SYSTEM_GET_NUM_HEADS,
                         &numHeadsParams, sizeof (numHeadsParams)));

    // get active displays and place the display mask of active display
    for (head = 0; head < numHeadsParams.numHeads; head++)
    {
        // hw active (vbios or vlcd enabled)
        dispActiveParams.subDeviceInstance = 0;
        dispActiveParams.head = head;
        dispActiveParams.flags = 0;

        CHECK_RC(pLwRm->Control(m_hDisplay,
                             LW0073_CTRL_CMD_SYSTEM_GET_ACTIVE,
                             &dispActiveParams, sizeof (dispActiveParams)));

        if (dispActiveParams.displayId)
        {
            hdcpInfoStruct.displayId = dispActiveParams.displayId;
        }
    }

    //
    // put the const value, a test vector for Cn 2c72677f652c2f27
    // in the HDCP input Cn
    //
    for (itempVar = 0; itempVar < LW0073_CTRL_CN_SIZE; itempVar++)
    {
        hdcpInfoStruct.Cn[itempVar] = arrayCn[itempVar];
    }

    //
    // put the const value, a test vector for Cksv b86646fc8c
    // in the HDCP input Cksv
    //
    for (iLoopVar = 0; iLoopVar < LW0073_CTRL_CN_SIZE; iLoopVar++)
    {
        hdcpInfoStruct.Cksv[iLoopVar] = arrayCksv[iLoopVar];
    }

     //
     // Set the maximum expected KSV list size so that API will compare
     // agianst this value
     //
     hdcpInfoStruct.actualKsvSize = LW0073_CTRL_MAX_HDCP_REPEATER_COUNT;

    // get HDCP repeater Information
    for (iLoopVar = 0; iLoopVar < iterations; iLoopVar++)
    {
        status = pLwRm->Control(m_hDisplay,
                       LW0073_CTRL_CMD_SPECIFIC_GET_HDCP_REPEATER_INFO,
                       &hdcpInfoStruct,
                       sizeof (hdcpInfoStruct)
                      );

        if (status == RC::LWRM_ILWALID_ARGUMENT)
        {
            return status;
        }
        if (status != OK)
        {
            Tasker::Sleep(pauseVal);
        }
        else
        {
            break;
        }
    }

    if (status != OK)
    {
        Printf(Tee::PriError,"Repeater timedout before getting KSV list\n"
                             "Return error\n");
        return RC::TIMEOUT_ERROR;
    }

    Printf(Tee::PriLow,"RepeaterTest: Got KSV FIFO List \n");

    for (iLoopVar = 0; iLoopVar < hdcpInfoStruct.actualKsvSize; iLoopVar++)
    {
       if ( !(iLoopVar%5) )
       {
           deviceCount++;
           Printf(Tee::PriLow,\
               "lwctrlRepeaterTest:Following are the Five \
               bytes data for DeviceCount 0x%x\n", deviceCount);
       }

       Printf(Tee::PriLow,"ksvList[0x%x] is 0x%x \n",\
              iLoopVar, hdcpInfoStruct.ksvList[iLoopVar]);
    }

    Printf(Tee::PriLow,"Value of Auth Required is 0x%x\n",\
           hdcpInfoStruct.bAuthrequired);
    Printf(Tee::PriLow,"Value BSTATUS is 0x%x\n", hdcpInfoStruct.BStatus);

    return rc;
}

//! \brief Cleanup Function.
//!
//! Cleanup all the allocated variables in the setup. Cleanup can be called
//! even in errorneous condition, in such case all the allocations should be
//! freed, this function takes care of such freeing up. As for NULL values free
//! will just return
//!
//! \sa Setup
//-----------------------------------------------------------------------------
RC HdcpRepeaterTest::Cleanup()
{
    LwRmPtr pLwRm;
    pLwRm->Free(m_hDisplay);

    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(HdcpRepeaterTest, RmTest, "HDCP Repeater test");

