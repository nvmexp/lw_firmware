/*
* Lwidia_COPYRIGHT_BEGIN
*
* Copyright 2009-2017,2019 by LWPU Corporation. All rights reserved.
* All information contained herein is proprietary and confidential
* to LWPU Corporation. Any use, reproduction, or disclosure without
* the written permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//! \file rmt_tlbstress.cpp
//! \brief Fermi test for TLB ilwalidates
//!
//! This tests is used to perform TLB ilwalidates for a given time with a given sleep time
//

#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "lwRmApi.h"
#include "core/include/utility.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"

#include "ctrl/ctrl2080.h"
#include "core/include/memcheck.h"

class TLBStressTest : public RmTest
{
public:
    TLBStressTest();
    virtual ~TLBStressTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    // binds test arguments (see rmtest.js) and class values
    SETGET_PROP(totalRunTimeSec, UINT32);
    SETGET_PROP(waitBetweenIlwalidateUs, UINT32);

private:
    LwRmPtr     pLwRm;

    UINT32      m_totalRunTimeSec;
    UINT32      m_waitBetweenIlwalidateUs;
};

//! \brief Class Constructor
//!
//! Indicate the name of the test.
//!
//! \sa Setup
//-----------------------------------------------------------------
TLBStressTest::TLBStressTest() :
    m_totalRunTimeSec(0),
    m_waitBetweenIlwalidateUs(0)
{
    SetName("TLBStressTest");
}

//! \brief Class Destructor
//!
//! \sa Cleanup
//------------------------------------------------------------------
TLBStressTest::~TLBStressTest()
{
}

//! \brief Whether the test is supported in current environment
//!
//! Test should run only on GPUs with MMU, thus a check for PAGING model.
//!
//! \return True if the test can be run in current environment
//!         False otherwise
//-------------------------------------------------------------------
string TLBStressTest::IsTestSupported()
{
    if (GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_PAGING))
    {
        return RUN_RMTEST_TRUE;
    }
    else
    {
        return "GPUSUB_SUPPORTS_PAGING Device feature is not supported on current platform";
    }
}

//! \brief Setup all necessary state before running the test.
//!
//! Setup internal state for the test.
//!
//! \return OK if there are no errors, or an error code.
//-------------------------------------------------------------------
RC TLBStressTest::Setup()
{
    RC          rc;

    CHECK_RC(InitFromJs());

    return rc;
}

//! \brief Run the TLB ilwalidates
//!
//!
//!
//-----------------------------------------------------------------------
RC TLBStressTest::Run()
{
    LW2080_CTRL_DMA_ILWALIDATE_TLB_PARAMS cmdParams = {0};

    UINT32 status = 0;
    UINT32 lwrTime = 0;
    UINT32 startTime = 0;

    // let it be 50 usec default wait if it is not set.
    if (m_waitBetweenIlwalidateUs == 0)
    {
        m_waitBetweenIlwalidateUs = 50;
    }

     //
     // Timing arguments are used by stress tests.
     // If m_totalRunTimeSec is set to 0, which is default value
     // then test does just one ilwalidate. This is a measure for RMTest suite just
     // to validate that TLB ilwalidate itself is not broken.
     //
     if (m_totalRunTimeSec == 0)
     {
         Printf(Tee::PriLow, "%s: Validating MMU once...\n", __FUNCTION__);
         Printf(Tee::PriLow, "%s: For running test a given time use totalRunTimeSec "
                             "and waitBetweenIlwalidateUs arguments.\n", __FUNCTION__);
         status = LwRmControl(pLwRm->GetClientHandle(), pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                              LW2080_CTRL_CMD_DMA_ILWALIDATE_TLB,
                              &cmdParams, sizeof (cmdParams));
         if (status != LW_OK)
         {
             Printf(Tee::PriLow, "%s: MMU ilwalidate failed...\n", __FUNCTION__);
             return status;
         }
     }
     else
     {
         startTime = Utility::GetSystemTimeInSeconds();
         Printf(Tee::PriLow,
                 "%s: Starting MMU ilwalidations for %d seconds with %d useconds sleep time.\n",
                 __FUNCTION__, m_totalRunTimeSec, m_waitBetweenIlwalidateUs);
         do
         {
             status = LwRmControl(pLwRm->GetClientHandle(), pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                                  LW2080_CTRL_CMD_DMA_ILWALIDATE_TLB,
                                  &cmdParams, sizeof (cmdParams));
             if (status != LW_OK)
             {
                 Printf(Tee::PriLow, "%s: MMU ilwalidate failed...\n", __FUNCTION__);
                 return status;
             }
             Utility::SleepUS(m_waitBetweenIlwalidateUs);
             lwrTime = Utility::GetSystemTimeInSeconds();
         } while (((lwrTime  - startTime) < m_totalRunTimeSec));
     }

     return status;
}

//! \brief Cleans up after the test.
//!
//! Cleanup all the allocated resources that the test selwred during Setup.
//! Cleanup can be called immediately if Setup fails or Run fails.
//!
//! \sa Setup
//---------------------------------------------------------------------------
RC TLBStressTest::Cleanup()
{
    RC rc;
    return rc;
}

//---------------------------------------------------------------------------
// JS Linkage
//---------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//!
//---------------------------------------------------------------------------
JS_CLASS_INHERIT(TLBStressTest, RmTest,
                 "TLBStressTest RMTest");

// binds test arguments (see rmtest.js) and class values
CLASS_PROP_READWRITE(TLBStressTest, totalRunTimeSec, UINT32,
                     "Total runtime of ilwalidation in SECONDS.");
CLASS_PROP_READWRITE(TLBStressTest, waitBetweenIlwalidateUs, UINT32,
                     "Time in MILLISECONDS for test sleep between MMU ilwalidations.");
