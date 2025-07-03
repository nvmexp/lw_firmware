/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2011,2013,2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_srmtest.cpp
//! \brief Test for validating an SRM.
//!
//! This test sends an SRM to RM for validation.

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "lwRmApi.h"
#include "lwmisc.h"
#include "gpu/perf/pmusub.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"

#include "class/cl0073.h"
#include "ctrl/ctrl0073.h"
#include "class/cl0073.h"  // LW04_DISPLAY_COMMON
#include "ctrl/ctrl85b6.h" // SUBDEVICE_PMU CTRL
#include "core/include/memcheck.h"

class SrmDispTest : public RmTest
{
public:
    SrmDispTest();
    virtual ~SrmDispTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    RC PollForResult(LwU32 *pResult);
    RC SendSrmOneTest();
    RC SendSrmMultTest();

    LwRm::Handle m_hDev;
    LwRm::Handle m_hDisplay;
    LwRm::Handle m_hSubDev;
    LwRm::Handle m_hClient;

    PMU *m_pPmu;
    LwU32                   m_pmuUcodeStateSaved;
    GpuSubdevice           *m_Parent;                     // GpuSubdevice that owns this PMU
};

//! \brief CtrlDispTest basic constructor
//!
//! This is just the basic constructor for the CtrlDispTest class.
//!
//! \sa Setup
//------------------------------------------------------------------------------
SrmDispTest::SrmDispTest()
{
    SetName("SrmDispTest");
    m_pPmu               = nullptr;
    m_hDev               = 0;
    m_hDisplay           = 0;
    m_hSubDev            = 0;
    m_hClient            = 0;
    m_pmuUcodeStateSaved = 0;
    m_Parent             = nullptr;
}

//! \brief CtrlDispTest destructor
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
SrmDispTest::~SrmDispTest()
{

}

//! \brief IsTestSupported : test for real hw
//!
//! For now test only designated to run on real silicon.  If someone
//! concludes it can be safely run on other elwironments then this
//! can be relaxed.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string SrmDispTest::IsTestSupported()
{
    //
    // This test and the feature have not been fully implemented/debugged.
    // We are lwrrently disabling this test suite by telling the tests that
    // all chips do not support this feature.
    //
    return "This test and the feature have not been fully implemented/debugged";

#if 0
    LwRmPtr      pLwRm;
    UINT32       status = 0;
    RC           rc;
    LW0073_CTRL_SYSTEM_GET_CAPS_V2_PARAMS dispCapsParams;

    if ( IsClassSupported( LW04_DISPLAY_COMMON ) == false )
        return false;

    //
    // Check if RM supports HDCP validation
    //
    m_hDev    = pLwRm->GetDeviceHandle(GetBoundGpuDevice());
    m_hSubDev = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    m_hClient = pLwRm->GetClientHandle();

    rc = pLwRm->Alloc(m_hDev, &m_hDisplay, LW04_DISPLAY_COMMON, NULL);
    if (OK != rc)
    {
        return false;
    }

    // zero out param structs
    memset(&dispCapsParams, 0, sizeof (dispCapsParams));

    // get display caps
    status = LwRmControl(m_hClient, m_hDisplay,
                         LW0073_CTRL_CMD_SYSTEM_GET_CAPS_V2,
                         &dispCapsParams, sizeof (dispCapsParams));
    if (status != LW_OK)
    {
        pLwRm->Free(m_hDisplay);
        m_hDisplay = 0;
        return false;
    }
    if (!LW0073_CTRL_SYSTEM_GET_CAP(dispCapsParams.capsTbl,
            LW0073_CTRL_SYSTEM_CAPS_KSV_SRM_VALIDATION_SUPPORTED))
    {
        pLwRm->Free(m_hDisplay);
        m_hDisplay = 0;
        return false;
    }

    return true;
#endif
}

//! \brief Setup entry point
//!
//! Allocate hClient (LW01_ROOT) object.
//!
//! \return OK if resource allocations succeed, error code indicating
//!         nature of failure otherwise
//-----------------------------------------------------------------------------
RC SrmDispTest::Setup()
{
    LwRmPtr pLwRm;
    RC      rc;

    CHECK_RC(InitFromJs());

    if (!m_hDev)
    {
        m_hDev = pLwRm->GetDeviceHandle(GetBoundGpuDevice());
    }
    if (!m_hSubDev)
    {
        m_hSubDev = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    }
    if (!m_hClient)
    {
        m_hClient = pLwRm->GetClientHandle();
    }
    if (!m_hDisplay)
    {
        rc = pLwRm->Alloc(m_hDev, &m_hDisplay, LW04_DISPLAY_COMMON, NULL);
        if (OK != rc)
        {
            Printf(Tee::PriHigh,"Could not allocate LW04_DISPLAY_COMMON\n");
            CHECK_RC(rc);
        }
    }

    m_Parent = GetBoundGpuSubdevice();

    // Get PMU class instance
    rc = (GetBoundGpuSubdevice()->GetPmu(&m_pPmu));

    if (OK != rc)
    {
        Printf(Tee::PriHigh,"PMU not supported\n");
        CHECK_RC(rc);
    }

    //
    // get and save the current PMU ucode state.
    // We should restore the PMU ucode to this state
    // after our tests are done.
    //
    UINT32 pmuUcodeState;
    rc = m_pPmu->GetUcodeState(&pmuUcodeState);
    if (OK != rc)
    {
        Printf(Tee::PriHigh,
               "%d: Failed to get PMU ucode state\n",
                __LINE__);
        CHECK_RC(rc);
    }

    m_pmuUcodeStateSaved = pmuUcodeState;

    return rc;
}

//! \brief Run entry point
//!
//! \return OK if test passes, error code indicating nature of failure
//!         otherwise
//-----------------------------------------------------------------------------
RC SrmDispTest::Run()
{
    RC rc;

    if (!m_hClient)
    {
        return (RC::DID_NOT_INITIALIZE_RM);
    }

    // Sleep here to allow the falcon to complete the HDCP INIT request
    Tasker::Sleep(1000);
    CHECK_RC(SendSrmOneTest());
    CHECK_RC(SendSrmMultTest());

    return rc;
}

//! \brief Cleanup entry point
//!
//! Release outstanding h_DisplayCmn (LW04_DISPLAY_COMMON) resource.
//!
//! \return No status returned; always passes.
//-----------------------------------------------------------------------------
RC SrmDispTest::Cleanup()
{
    LwRmPtr pLwRm;

    RC     rc;
    UINT32 ucodeState;

    CHECK_RC(m_pPmu->GetUcodeState(&ucodeState));

    //
    // if current ucode state is not the same as the initial saved ucode state
    // then bring the PMU to the initial ucode state
    //
    if( ucodeState != m_pmuUcodeStateSaved )
    {
        Printf(Tee::PriHigh,
               "%d:SrmDispTest::%s: Trying to restore PMU state\n",
                __LINE__,__FUNCTION__);
        switch (m_pmuUcodeStateSaved)
        {
            case LW85B6_CTRL_PMU_UCODE_STATE_READY:
            case LW85B6_CTRL_PMU_UCODE_STATE_RUNNING:
            {
                CHECK_RC(m_pPmu->Reset(true));
                break;
            }

            case LW85B6_CTRL_PMU_UCODE_STATE_NONE:
            case LW85B6_CTRL_PMU_UCODE_STATE_LOADED:
            {
                CHECK_RC(m_pPmu->Reset(false));
                break;
            }
            default:
            {
                //
                // This is never expected to occur. Printing error message
                // and returning LWRM_ILWALID_DATA
                //
                Printf(Tee::PriHigh,
                    "%d:SrmDispTest::%s: Error retrieving saved ucode state\n",
                        __LINE__,__FUNCTION__);
                return RC::LWRM_ILWALID_DATA;
            }
        }
        Printf(Tee::PriHigh,
               "%d:SrmDispTest::%s: Restored initial PMU state\n",
                __LINE__,__FUNCTION__);
    }
    else
    {
        Printf(Tee::PriHigh,
               "%d:SrmDispTest::%s: No need to restore PMU ucode state\n",
                __LINE__,__FUNCTION__);
    }

    if (m_hDisplay)
    {
        pLwRm->Free(m_hDisplay);
    }

    return OK;
}

//-----------------------------------------------------------------------------
// Private Member Functions
//-----------------------------------------------------------------------------

//! \brief Poll to retrieve the last result for SRM validation
//!
//! If the request completes successfully, the status of the last
//! LW0073_CTRL_CMD_SYSTEM_SET_SRM request is returned through pStatus.
//!
//! \param pStatus  : pointer through which the status of the last
//!                   LW0073_CTRL_CMD_SYSTEM_SET_SRM request is returned.
//!
//! \return OK if the request passes
//!         TIMEOUT if RM fails to complete the
//!                 LW0073_CTRL_CMD_SYSTEM_VALIDATE_SRM request within the
//!                 time allowed
//!         status from the LW0073_CTRL_CMD_SYSTEM_GET_SRM_STATUS request
//!         otherwise.
RC SrmDispTest::PollForResult(LwU32 *pResult)
{
    LW0073_CTRL_SYSTEM_GET_SRM_STATUS_PARAMS dispSrmStatusParams;
    LwU32 status = 0;
    LwU32 i;

    for (i = 0; i < 3; i++)
    {
        Tasker::Sleep(1000);
        status = LwRmControl(m_hClient, m_hDisplay,
                             LW0073_CTRL_CMD_SYSTEM_GET_SRM_STATUS,
                             &dispSrmStatusParams,
                             sizeof (dispSrmStatusParams));
        if (LW_ERR_NOT_READY != status)
        {
            break;
        }
    }

    if (LW_ERR_NOT_READY == status)
    {
        return LW_ERR_TIMEOUT;
    }
    else if (LW_OK == status)
    {
        *pResult = dispSrmStatusParams.status;
    }
    return status;
}

LwU8 SRM[] = {0x80, 0x00, 0x00, 0x05, 0x01, 0x00, 0x00, 0x36,
              0x02, 0x51, 0x1e, 0xf2, 0x1a, 0xcd, 0xe7, 0x26,
              0x97, 0xf4, 0x01, 0x97, 0x10, 0x19, 0x92, 0x53,
              0xe9, 0xf0, 0x59, 0x95, 0xa3, 0x7a, 0x3b, 0xfe,
              0xe0, 0x9c, 0x76, 0xdd, 0x83, 0xaa, 0xc2, 0x5b,
              0x24, 0xb3, 0x36, 0x84, 0x94, 0x75, 0x34, 0xdb,
              0x10, 0x9e, 0x3b, 0x23, 0x13, 0xd8, 0x7a, 0xc2,
              0x30, 0x79, 0x84};

//! \brief SrmDispTest routine
//!
//! This function covers the following LW04_DISPLAY_COMMON control commands:
//!
//!  LW0073_CTRL_CMD_SYSTEM_SET_SRM
//!
//!  Tests a correct SRM sent in one chunk
//!
//! \return OK if the test passes, subtest-specific value indicating
//!         nature of failure otherwise
RC SrmDispTest::SendSrmOneTest()
{
    LW0073_CTRL_SYSTEM_VALIDATE_SRM_PARAMS dispSrmParams;
    LwU32 status = 0;
    LwU32 result;
    RC    rc = RmApiStatusToModsRC(LW_ERR_GENERIC);

    Printf(Tee::PriLow, "SendSrmTest: testing send SRM One cmd\n");

    // zero out param structs
    memset(&dispSrmParams, 0,
           sizeof (LW0073_CTRL_SYSTEM_VALIDATE_SRM_PARAMS));

    dispSrmParams.srm.startByte = 0;
    dispSrmParams.srm.numBytes = 59;
    dispSrmParams.srm.totalBytes = 59;
    memcpy(dispSrmParams.srm.srmBuffer, SRM, 59);
    status = LwRmControl(m_hClient, m_hDisplay,
                         LW0073_CTRL_CMD_SYSTEM_VALIDATE_SRM,
                         &dispSrmParams, sizeof (dispSrmParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriNormal,
               "SendSrmTest: failed: 0x%x\n", (UINT32)status);
        return RmApiStatusToModsRC(status);
    }

    // Wait for RM to process PMU messages

    Printf(Tee::PriNormal,
        "\n\n*** SendSrmTest: Waiting for PMU... ***\n\n");

    status = PollForResult(&result);
    switch (status)
    {
        case LW_OK:
            if (LW0073_CTRL_SYSTEM_SRM_STATUS_OK == result)
            {
                rc = RmApiStatusToModsRC(LW_OK);
                Printf(Tee::PriLow, "SendSrmTest: passed\n");
            }
            else
            {
                rc = RmApiStatusToModsRC(LW_ERR_GENERIC);
                Printf(Tee::PriLow,
                    "SendSrmTest: SRM validation failed: 0x%x\n", (UINT32)result);
            }
            break;

        case LW_ERR_TIMEOUT:
            rc = RmApiStatusToModsRC(LW_ERR_TIMEOUT);
            Printf(Tee::PriLow, "SendSrmTest: timed out\n");
            break;

        default:
            rc = RmApiStatusToModsRC(LW_ERR_GENERIC);
            Printf(Tee::PriLow, "SendSrmTest: failed\n");
            break;
    };

    return rc;
}

//! \brief SrmDispTest routine
//!
//! This function covers the following LW04_DISPLAY_COMMON control commands:
//!
//!  LW0073_CTRL_CMD_SYSTEM_SET_SRM
//!
//!  Tests a correct SRM sent in multiple chunks
//!
//! \return OK if the test passes, subtest-specific value indicating
//!         nature of failure otherwise
RC SrmDispTest::SendSrmMultTest()
{
    LW0073_CTRL_SYSTEM_VALIDATE_SRM_PARAMS dispSrmParams;
    LwU32 status = 0;
    LwU32 result;
    LwU32 bytesRemaining = 59;
    RC    rc = RmApiStatusToModsRC(LW_ERR_GENERIC);

    Printf(Tee::PriLow, "SendSrmTest: testing send SRM Multiple cmd\n");

    // zero out param structs
    memset(&dispSrmParams, 0,
           sizeof (LW0073_CTRL_SYSTEM_VALIDATE_SRM_PARAMS));

    dispSrmParams.srm.startByte = 0;
    dispSrmParams.srm.totalBytes = 59;
    dispSrmParams.srm.numBytes = 16;

    // Send the SRM down in multiple chunks
    while (bytesRemaining)
    {
        memcpy(dispSrmParams.srm.srmBuffer, &SRM[dispSrmParams.srm.startByte], dispSrmParams.srm.numBytes);
        status = LwRmControl(m_hClient, m_hDisplay,
                             LW0073_CTRL_CMD_SYSTEM_VALIDATE_SRM,
                             &dispSrmParams, sizeof (dispSrmParams));
        if (status != LW_OK)
        {
            Printf(Tee::PriNormal,
                   "SendSrmTest: failed: 0x%x\n", (UINT32)status);
            return RmApiStatusToModsRC(status);
        }

        // Set up for the next chunk
        dispSrmParams.srm.startByte += dispSrmParams.srm.numBytes;
        bytesRemaining -= dispSrmParams.srm.numBytes;
        if (bytesRemaining && (bytesRemaining < dispSrmParams.srm.numBytes))
        {
            dispSrmParams.srm.numBytes = bytesRemaining % 16;
        }
    }

    // Wait for RM to process PMU messages

    Printf(Tee::PriNormal,
        "\n\n*** SendSrmTest: Waiting for PMU... ***\n\n");

    status = PollForResult(&result);
    switch (status)
    {
        case LW_OK:
            if (LW0073_CTRL_SYSTEM_SRM_STATUS_OK == result)
            {
                rc = RmApiStatusToModsRC(LW_OK);
                Printf(Tee::PriLow, "SendSrmTest: passed\n");
            }
            else
            {
                rc = RmApiStatusToModsRC(LW_ERR_GENERIC);
                Printf(Tee::PriLow,
                    "SendSrmTest: SRM validation failed: 0x%x\n", (UINT32)result);
            }
            break;

        case LW_ERR_TIMEOUT:
            rc = RmApiStatusToModsRC(LW_ERR_TIMEOUT);
            Printf(Tee::PriLow, "SendSrmTest: timed out\n");
            break;

        default:
            rc = RmApiStatusToModsRC(LW_ERR_GENERIC);
            Printf(Tee::PriLow, "SendSrmTest: failed\n");
            break;
    };

    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ CtrlDispTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(SrmDispTest, RmTest,
                 "SrmDispTest RM Test.");
