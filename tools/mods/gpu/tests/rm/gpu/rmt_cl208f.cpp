/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_cl208f.cpp
//! \brief To verify basic functionality of new class LW20_SUBDEVICE_DIAG
//!
//! Basic functioanlity verified here is the proper hooking up of the
//! class LW20_SUBDEVICE_DIAG within the Device-Subdevice relationship.
//! the subdevice acts as the parent to the object of LW20_SUBDEVICE_DIAG.
//! Basic functionality tested is:
//! 1. Class allocation with subdevice as parent:
//!    LwRmAlloc(hClient, hSubDev, hSubDevDiag,LW20_SUBDEVICE_DIAG,<Param208f>)
//! 2. Rm Control hook for calling in Diag specific functions.
//!    LwRmControl(hClient, hSubdevDiag, cmd, pParams, paramsSize);
//! 3. Class freeing up: Diag freeing done as a part of SubDevice free-up
//!    LwRmFree(m_hClient, hDev, hSubDev);

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"

#include "gpu/utility/rmclkutil.h"
#include "lwRmApi.h"
#include "ctrl/ctrl0000.h"
#include "ctrl/ctrl0080.h"
#include "class/cl208f.h" // LW20_SUBDEVICE_DIAG
#include "ctrl/ctrl208f.h" // LW20_SUBDEVICE_DIAG CTRL
#include "core/include/memcheck.h"

// Defines for the handles
#define LWCTRL_DEV_HANDLE                (0xbb008f00)
#define LWCTRL_SUBDEV_HANDLE             (0xbb208000)
#define LWCTRL_SUBDEVDIAG_HANDLE         (0xbb208f00)
#define LWCTRL_SUBDEVDIAG_HANDLE2        (0xbb208ff0)

class Class208fTest : public RmTest
{
public:
    Class208fTest();
    virtual ~Class208fTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    LwRm::Handle m_hClient;
    LwRm::Handle m_hDevice;
    LwRm::Handle m_hSubDev;
    LwRm::Handle m_hSubDevDiag;
    LwRm::Handle m_hSubDevDiag2;

    RC Class208fAllocControl(UINT32 hParent,
                             LW0000_CTRL_GPU_GET_ID_INFO_PARAMS
                              *gpuIdInfoParams);
};

//! \brief Class208fTest constructor
//!
//! Placeholder : doesn't do much, much funcationality in Setup()
//!
//! \sa Setup
//-----------------------------------------------------------------------------
Class208fTest::Class208fTest() :
    m_hClient(0),
    m_hDevice(0),
    m_hSubDev(0),
    m_hSubDevDiag(0),
    m_hSubDevDiag2(0)
{
    SetName("Class208fTest");
}

//! \brief Class208fTest destructor
//!
//! Placeholder : doesn't do much, much funcationality in Cleanup()
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
Class208fTest::~Class208fTest()
{

}

//! \brief IsTestSupported(), Looks for whether test can execute in current elw.
//!
//! The test is basic Diag class and will be available on any HW/SW elwiornment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//-----------------------------------------------------------------------------
string Class208fTest::IsTestSupported()
{
    return RUN_RMTEST_TRUE;
}

//! \brief Setup(): Generally used for any test level allocation
//!
//! Here the allocation and usage is done in Run(), lwrrently only one
//! Subdevice default but placed eveything in Run to make it extensible
//! otherwise for single subdevicediag within subdevice the setup can be used
//! for allocation.
//
//! \return OK as does nothing.
//! \sa Run()
//-----------------------------------------------------------------------------
RC Class208fTest::Setup()
{
    RC rc;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    m_hDevice = LWCTRL_DEV_HANDLE;
    m_hSubDev = LWCTRL_SUBDEV_HANDLE;
    m_hSubDevDiag = LWCTRL_SUBDEVDIAG_HANDLE;
    m_hSubDevDiag2 = LWCTRL_SUBDEVDIAG_HANDLE2;

    return rc;
}

//! \brief Run(): Used generally for placing all the testing stuff.
//!
//! Run() as said in Setup() has all allocations and control tests for one
//! subdevicediag, placed in this way to make it easy for multiple subdevice
//! support easy.
//!
//! \return OK if the passed, specific RC if failed
//! \sa Setup()
//-----------------------------------------------------------------------------
RC Class208fTest::Run()
{
    RC rc;
    LW0000_CTRL_GPU_GET_ATTACHED_IDS_PARAMS gpuAttachedIdsParams = {{0}};
    LW0000_CTRL_GPU_GET_ID_INFO_PARAMS gpuIdInfoParams = {0};
    LW0080_ALLOC_PARAMETERS lw0080Params = {0};
    UINT32 hDev, iLoopVar;
    INT32 status;

    status = (UINT32)LwRmAllocRoot((LwU32*)&m_hClient);
    rc = RmApiStatusToModsRC(status);
    CHECK_RC(rc);

    // get attached gpus ids
    status = LwRmControl(m_hClient, m_hClient,
                         LW0000_CTRL_CMD_GPU_GET_ATTACHED_IDS,
                         &gpuAttachedIdsParams, sizeof (gpuAttachedIdsParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,
            "%d:GET_IDS failed: 0x%x \n", __LINE__,status);
        return RmApiStatusToModsRC(status);
    }

    // for each attached gpu...
    gpuIdInfoParams.szName = (LwP64)NULL;
    for (iLoopVar = 0; iLoopVar < LW0000_CTRL_GPU_MAX_ATTACHED_GPUS;iLoopVar++)
    {
        if (gpuAttachedIdsParams.gpuIds[iLoopVar] == 0xffffffff)
            break;

        // get gpu instance info
        gpuIdInfoParams.gpuId = gpuAttachedIdsParams.gpuIds[iLoopVar];
        status = LwRmControl(m_hClient, m_hClient,
                             LW0000_CTRL_CMD_GPU_GET_ID_INFO,
                             &gpuIdInfoParams, sizeof (gpuIdInfoParams));
        if (status != LW_OK)
        {
            Printf(Tee::PriHigh,
                "%d: GET_ID_INFO failed: 0x%x, \n",__LINE__,status);
            return RmApiStatusToModsRC(status);
        }

        // allocate the gpu's device.
        lw0080Params.deviceId = gpuIdInfoParams.deviceInstance;
        lw0080Params.hClientShare = 0;
        hDev = m_hDevice + gpuIdInfoParams.deviceInstance;
        status = LwRmAlloc(m_hClient, m_hClient, hDev,
                           LW01_DEVICE_0,
                           &lw0080Params);
        if (status != LW_OK)
        {
            Printf(Tee::PriHigh,
                   "%d: Dev Alloc failed!\n",
                            __LINE__);
            return RmApiStatusToModsRC(status);
        }

        // alloc & control SubdeviceDiag with device-subdevice parent
        status  = Class208fAllocControl(hDev, &gpuIdInfoParams);
        if (status != LW_OK)
        {
            Printf(Tee::PriHigh,
                   "%d: Class208fAllocControl failed: status 0x%x\n",
                    __LINE__,status);
            return RmApiStatusToModsRC(status);
        }

        // free handles
        LwRmFree(m_hClient, m_hClient, hDev);
    }
    LwRmFree(m_hClient, m_hClient, m_hClient);
    return rc;
}

//! \brief Cleanup()
//!
//! As everything done in Run (lwrrently) thsi cleanup acts again like a
//! placeholder.
//! \sa Run()
//-----------------------------------------------------------------------------
RC Class208fTest::Cleanup()
{
    return OK;
}

//-----------------------------------------------------------------------------
// Private Member Functions
//-----------------------------------------------------------------------------

//! \brief Class208fAllocControl()
//!
//! Run() as said in Setup() has all allocations and control tests for all
//! subdevice-subdevicediag, Placed in the loop over all available subdevice
//! in the current device
//!
//! \return OK if the passed, specific RC if failed
//! \sa Run()
//-----------------------------------------------------------------------------
RC Class208fTest::Class208fAllocControl(UINT32 hParent,
                                        LW0000_CTRL_GPU_GET_ID_INFO_PARAMS
                                          *gpuIdInfoParams)
{
    RC rc;
    RC failCaseRc;
    LW0080_CTRL_GPU_GET_NUM_SUBDEVICES_PARAMS numSubDevicesParams = {0};
    LW2080_ALLOC_PARAMETERS lw2080Params = {0};
    UINT32 hSubDev, hSubDevDiag, hSubDevDiag2;
    UINT32 subdev;
    INT32 status;

    // get number of subdevices
    status = LwRmControl(m_hClient, hParent,
                         LW0080_CTRL_CMD_GPU_GET_NUM_SUBDEVICES,
                         &numSubDevicesParams, sizeof (numSubDevicesParams));
    if (status != LW_OK)
    {
        printf("subdevice cnt cmd failed: 0x%x\n", status);
        return status;
    }

    // do unicast (subdevice) commands
    for (subdev = 0; subdev < numSubDevicesParams.numSubDevices; subdev++)
    {
        // allocate subdevice
        hSubDev = m_hSubDev + gpuIdInfoParams->subDeviceInstance;
        lw2080Params.subDeviceId = gpuIdInfoParams->subDeviceInstance;
        status = LwRmAlloc(m_hClient, hParent, hSubDev,
                           LW20_SUBDEVICE_0,
                           &lw2080Params);
        if (status != LW_OK)
        {
            Printf(Tee::PriHigh,
                   "%d: LwRmAlloc(SubDevice) failed: status 0x%x\n",
                    __LINE__,status);
            return RmApiStatusToModsRC(status);
        }

        hSubDevDiag = m_hSubDevDiag + gpuIdInfoParams->subDeviceInstance;
        status = LwRmAlloc(m_hClient, hSubDev, hSubDevDiag,
                           LW20_SUBDEVICE_DIAG,
                           NULL);
        if (status != LW_OK)
        {
            Printf(Tee::PriHigh,
                   "%d: LwRmAlloc(SubDevice Diag) failed: status 0x%x\n",
                    __LINE__,status);
            return RmApiStatusToModsRC(status);
        }

        //
        // NULL CMD Check, Called in this way:
        // LwRmControl(hClient, hSubdeviceDiag, cmd, pParams, paramsSize);
        //
        status = LwRmControl(m_hClient, hSubDevDiag,
                             0,
                             0,
                             0);
        if (status != LW_OK)
        {
            Printf(Tee::PriHigh,
                "%d: LW20_SUBDEVICE_DIAG NULL CTRL CMD failed: 0x%x \n",
                  __LINE__,status);
            CHECK_RC(RmApiStatusToModsRC(status));
        }

        // Try allocating 2nd time without freeing, this is expected to fail
        hSubDevDiag2 = m_hSubDevDiag2 + gpuIdInfoParams->subDeviceInstance;
        status = LwRmAlloc(m_hClient, hSubDev, hSubDevDiag2,
                           LW20_SUBDEVICE_DIAG,
                           NULL);
        if (status != LW_OK)
        {
            Printf(Tee::PriLow,
                   "%d:2nd time LwRmAlloc(SubDevice Diag) failed: status 0x%x\n",
                    __LINE__,status);
            failCaseRc = RmApiStatusToModsRC(status);
            Printf(Tee::PriLow,
                   "%d: rc message is  %s\n", __LINE__,failCaseRc.Message());
        }

        // Trying on free cases

        // hSubDevDiag freeing, hSubDev as the parent
        LwRmFree(m_hClient, hSubDev, hSubDevDiag);

        // Try allocating again, as we freed up this must be allocated
        status = LwRmAlloc(m_hClient, hSubDev, hSubDevDiag2,
                           LW20_SUBDEVICE_DIAG,
                           NULL);
        if (status != LW_OK)
        {
            Printf(Tee::PriHigh,
                   "%d: This should not happen\n", __LINE__);
            Printf(Tee::PriHigh,
                   "%d:3rd time After free up Diag alloc fail: status 0x%x\n",
                    __LINE__,status);
            return RmApiStatusToModsRC(status);
        }

        // Free hSubDevDiag again, hSubDev as the parent
        LwRmFree(m_hClient, hSubDev, hSubDevDiag2);

        //
        // Try Freeing up hSubDevDiag again, just for completeness seck
        // Expected to fail so checking
        //
        status = LwRmFree(m_hClient, hSubDev, hSubDevDiag2);
        if (status != LW_OK)
        {
            Printf(Tee::PriLow,
                   "%d: Execpted to fail here\n",__LINE__);
            Printf(Tee::PriLow,
                   "%d: Freeing already freed SubdeviceDiag:status 0x%x\n",
                    __LINE__,status);
            failCaseRc = RmApiStatusToModsRC(status);
            Printf(Tee::PriLow,
                   "%d: rc message is  %s\n", __LINE__,failCaseRc.Message());
        }

        // hSubDev freeing frees the associated subdeviceDiag
        status = LwRmAlloc(m_hClient, hSubDev, hSubDevDiag,
                           LW20_SUBDEVICE_DIAG,
                           NULL);
        if (status != LW_OK)
        {
            Printf(Tee::PriHigh,
                   "%d: This should not happen\n", __LINE__);
            Printf(Tee::PriHigh, "%d: Diag alloc fail: status 0x%x\n", __LINE__,status);
            return RmApiStatusToModsRC(status);
        }

        LwRmFree(m_hClient, hParent, hSubDev);
    }
    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ Class208fTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(Class208fTest, RmTest,
"Test new class addition, class named 208f, class used specifically for DIAG");

