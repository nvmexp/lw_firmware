/*
* Lwidia_COPYRIGHT_BEGIN
*
* Copyright 2017,2019 by LWPU Corporation. All rights reserved.
* All information contained herein is proprietary and confidential
* to LWPU Corporation. Any use, reproduction, or disclosure without
* the written permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//! \file rmt_lwhdmiUserSpaceTest.cpp
//! \brief This test verifies the alloc/free/mapping/unmapping functions that
//!        client uses to write into HDMI 4k space for LwDisplay.
//!
//

#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "lwos.h"
#include "core/include/lwrm.h"
#include "lwRmApi.h"
#include "core/include/platform.h"
#include "gpu/include/gpudev.h"

#include "class/clc370.h"   // LWC370_DISPLAY
#include "class/clc371.h"   // LWC371_DISP_SF_USER
#include "disp/v03_00/dev_disp.h"
#include "gpu/display/lwdisplay/lw_disp.h"
#include "core/include/memcheck.h"

class LwHdmiUserSpaceTest : public RmTest
{
public:
    LwHdmiUserSpaceTest();
    virtual ~LwHdmiUserSpaceTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
};

//! \brief Class Constructor
//!
//! Indicate the name of the test.
//!
//! \sa Setup
//-----------------------------------------------------------------
LwHdmiUserSpaceTest::LwHdmiUserSpaceTest()
{
    SetName("LwHdmiUserSpaceTest");
}

//! \brief Class Destructor
//!
//! \sa Cleanup
//------------------------------------------------------------------
LwHdmiUserSpaceTest::~LwHdmiUserSpaceTest()
{

}

//! \brief Whether the test is supported in current environment
//!
//! \return True if the test can be run in current environment
//!         False otherwise
//-------------------------------------------------------------------
string LwHdmiUserSpaceTest::IsTestSupported()
{
    LwRmPtr pLwRm;

    if(pLwRm->IsClassSupported(LWC371_DISP_SF_USER, GetBoundGpuDevice()))
        return RUN_RMTEST_TRUE;

    return "LWC371_DISP_SF_USER class is not supported on current chip";
}

//! \brief Setup all necessary state before running the test.
//!
//! Setup internal state for the test.
//!
//! \return OK if there are no errors, or an error code.
//-------------------------------------------------------------------
RC LwHdmiUserSpaceTest::Setup()
{
   InitFromJs();

   return OK;
}

//! \brief Run the hdmiUserSpaceTest
//!
//! It writes the memory location after mapping and checks through register
//! read whether it matches or not.
//!
//-----------------------------------------------------------------------
RC LwHdmiUserSpaceTest::Run()
{
    GpuSubdevice *pSubdev = GetBoundGpuSubdevice();

    // Take the diplay class to determine which SF class we have to use
    Display *pDisplay = GetDisplay();
    volatile  LwC371DispSfUserMap  *pDispSfUser;
    LwRm::Handle hDispSfUser = 0;
    LwRmPtr   pLwRm;
    RC rc;
    LwU32 i, j;
    LwU32 temp;
    LwU32 memValue, regValue;
    LwU32 offsetIndex;
    LwU32 dispSfClass = 0;
    Printf(Tee::PriLow,
           "%s: LwHdmiUserSpaceTest starts!\n", __FUNCTION__);

    UINT32 dispClass = pDisplay->GetClass();

    switch (dispClass)
    {
        case LWC370_DISPLAY:
            dispSfClass = LWC371_DISP_SF_USER;
            break;

        default:
            Printf(Tee::PriHigh, "%s: UNSUPPORTED DISPLAY CLASS\n", __FUNCTION__);
            rc = RC::LWRM_ILWALID_CLASS;
    }

    CHECK_RC_CLEANUP(
        pLwRm->Alloc(pLwRm->GetSubdeviceHandle(pSubdev),
        &hDispSfUser,
        dispSfClass,
        NULL)
        );

    CHECK_RC_CLEANUP(pLwRm->MapMemory(
        hDispSfUser, 0, sizeof(LwC371DispSfUserMap), (void **)&pDispSfUser, 0, GetBoundGpuSubdevice()));

    for (i = 0; i < LW_PDISP_SF_HDMI_INFO_CTRL__SIZE_1 ; i++)
    {
        for (j = 0; j < LW_PDISP_SF_HDMI_INFO_CTRL__SIZE_2; j++)
        {
            offsetIndex = (LW_PDISP_SF_HDMI_INFO_CTRL(i,j) - LW_PDISP_SF_HDMI_INFO_CTRL(0,0)) / 4;
            temp = MEM_RD32(&pDispSfUser->dispSfUserOffset[offsetIndex]);
            MEM_WR32(&pDispSfUser->dispSfUserOffset[offsetIndex], ~temp);
            memValue = MEM_RD32(&pDispSfUser->dispSfUserOffset[offsetIndex]);
            regValue = pSubdev->RegRd32(LW_PDISP_SF_HDMI_INFO_CTRL(i,j));

            if (memValue == regValue)
            {
               Printf(Tee::PriLow, "%s: Matched at i = %d and  j = %d \n", __FUNCTION__, i , j);
            }
            else
            {
               Printf(Tee::PriHigh,
                   "%s: Didn't match the memory read and register read for i = %d and j = %d \n"
                    , __FUNCTION__, i , j);

               rc = RC::DATA_MISMATCH;
               break;
            }
            //Restoring to original value
            MEM_WR32(&pDispSfUser->dispSfUserOffset[offsetIndex], temp);
            if ( temp != (pSubdev->RegRd32(LW_PDISP_SF_HDMI_INFO_CTRL(i,j))))
            {
                Printf(Tee::PriHigh, "%s: MEM write failed at i = %d and  j = %d \n", __FUNCTION__, i , j);
                rc = RC::DATA_MISMATCH;
                break;
            }
        }
    }
    pLwRm->UnmapMemory(hDispSfUser, pDispSfUser, 0, GetBoundGpuSubdevice());

Cleanup:

    pLwRm->Free(hDispSfUser);
    return rc;
}

//! \brief Cleans up after the test.
//!
//! Cleanup all the allocated resources that the test selwred during Setup.
//! Cleanup can be called immediately if Setup fails or Run fails.
//!
//! \sa Setup
//---------------------------------------------------------------------------
RC LwHdmiUserSpaceTest::Cleanup()
{
    return OK;
}

//---------------------------------------------------------------------------
// JS Linkage
//---------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//!
//---------------------------------------------------------------------------
JS_CLASS_INHERIT(LwHdmiUserSpaceTest, RmTest,
                 "LwHdmiUserSpaceTest RMTest");

