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
//! \file rmt_ctrldisp.cpp
//! \brief Test for covering display object control commands.
//!
//! This test allocates display objects (LW04_DISPLAY_COMMON
//! if supported) on all available devices and
//! issues a sequence of LwRmControl commands that validate both
//! correct operation as well as various error handling conditions.
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "lwRmApi.h"
#include "lwmisc.h"

#include "class/cl0073.h"
#include "ctrl/ctrl0000.h"
#include "ctrl/ctrl0073.h"
#include "ctrl/ctrl0080.h"
#include "class/cl0073.h" // LW04_DISPLAY_COMMON
#include "core/include/memcheck.h"

class CtrlDispTest : public RmTest
{
public:
    CtrlDispTest();
    virtual ~CtrlDispTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    RC DispCmnTest(UINT32 subdev);

    RC DispCmnCapsTest();
    RC DispCmnSupportedTest(UINT32 subdev,
                            UINT32 *pSupportedMask);
    RC DispCmnConnectedTest(UINT32 subdev,
                            UINT32 supportedMask,
                            UINT32 *pConnnectedMask);
    RC DispCmnDisplayIdTest(UINT32 subdev,
                            UINT32 connectedMask);
    RC DispCmnHeadRoutingTest(UINT32 subdev,
                              UINT32 supportedMask,
                              UINT32 connnectedMask);
    RC DispCmnActiveTest(UINT32 subdev);
    RC DispCmnConnectorTableTest(UINT32 subdev);

    LwRm::Handle m_hClient = 0;
    LwRm::Handle m_hDev = 0;
    static const LwRm::Handle m_hDevice = 0xbabe8000;
    static const LwRm::Handle m_hDisplayCmn = 0xbabe7300;

    bool m_isDeviceAllocated = false;
    bool m_isDisplayCmnAllocated = false;
};

//! \brief CtrlDispTest basic constructor
//!
//! This is just the basic constructor for the CtrlDispTest class.
//!
//! \sa Setup
//------------------------------------------------------------------------------
CtrlDispTest::CtrlDispTest()
{
    SetName("CtrlDispTest");
}

//! \brief CtrlDispTest destructor
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
CtrlDispTest::~CtrlDispTest()
{

}

//! \brief IsSupported : test for real hw
//!
//! For now test only designated to run on real silicon.  If someone
//! concludes it can be safely run on other elwironments then this
//! can be relaxed.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string CtrlDispTest::IsTestSupported()
{

    static const UINT32 DisplayClasses[] =
        {
            LW04_DISPLAY_COMMON
        };
    UINT32 numDisplayClasses = sizeof( DisplayClasses ) / sizeof( DisplayClasses[0] );

    for ( UINT32 index = 0; index < numDisplayClasses; index++ )
    {
        if (IsClassSupported( DisplayClasses[index] ) == true )
            return RUN_RMTEST_TRUE;
    }

    return "No required Dispaly Class is supported on current platform";
}

//! \brief Setup entry point
//!
//! Allocate hClient (LW01_ROOT) object.
//!
//! \return OK if resource allocations succeed, error code indicating
//!         nature of failure otherwise
//-----------------------------------------------------------------------------
RC CtrlDispTest::Setup()
{
    RC rc;
    UINT32 status;

    m_hClient = 0;
    status = (UINT32)LwRmAllocRoot((LwU32*)&m_hClient);
    CHECK_RC(RmApiStatusToModsRC(status));

    return rc;
}

//! \brief Run entry point
//!
//! Allocate h_Device (LW01_DEVICE_0) and h_DisplayCmn (LW04_DISPLAY_COMMON)
//! objects.
//!
//! For each subdevice within h_Device, initiate the h_DisplayCmn tests.
//!
//! \return OK if test passes, error code indicating nature of failure
//!         otherwise
//-----------------------------------------------------------------------------
RC CtrlDispTest::Run()
{
    LW0000_CTRL_GPU_GET_DEVICE_IDS_PARAMS deviceIdsParams = {0};
    LW0080_ALLOC_PARAMETERS lw0080Params = {0};
    LW0080_CTRL_GPU_GET_NUM_SUBDEVICES_PARAMS numSubDevicesParams = {0};
    UINT32 status = 0;
    UINT32 i, subdev;
    RC rc;

    if (!m_hClient)
    {
        return (RC::DID_NOT_INITIALIZE_RM);
    }

    // get valid device instances mask
    status = LwRmControl(m_hClient, m_hClient,
                         LW0000_CTRL_CMD_GPU_GET_DEVICE_IDS,
                         &deviceIdsParams, sizeof (deviceIdsParams));

    if (status != LW_OK)
    {
        Printf(Tee::PriNormal,
               "CtrlDispTest: GET_DEVICE_IDS failed: 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }

    // for each device...
    for (i = 0; i < LW0080_MAX_DEVICES; i++)
    {
        // skip over invalid instances
        if ((deviceIdsParams.deviceIds & (1 << i)) == 0)
        {
            continue;
        }

        Printf(Tee::PriLow, "CtrlDispTest: testing deviceInst %d\n",
               (UINT32)deviceIdsParams.deviceIds);

        // assume allocation failures
        m_isDeviceAllocated = false;
        m_isDisplayCmnAllocated = false;

        // allocate device
        lw0080Params.deviceId = i;
        lw0080Params.hClientShare = 0;
        m_hDev = m_hDevice + i;
        status = LwRmAlloc(m_hClient, m_hClient, m_hDev,
                           LW01_DEVICE_0,
                           &lw0080Params);
        if (status != LW_OK)
        {
            Printf(Tee::PriNormal,
                   "CtrlDispTest: LwRmAlloc(LW01_DEVICE): id=%d failed!\n", i);
            return RmApiStatusToModsRC(status);
        }

        m_isDeviceAllocated = true;

        // allocate LW04_DISPLAY_COMMON
        status = LwRmAlloc(m_hClient, m_hDev, m_hDisplayCmn,
                           LW04_DISPLAY_COMMON, NULL);
        if (status != LW_OK)
        {
            Printf(Tee::PriNormal,
                   "CtrlDispTest: LwRmAlloc(LW04_DISPLAY_COMMON) failed: "
                   "status 0x%x\n", status);
            return RmApiStatusToModsRC(status);
        }

        m_isDisplayCmnAllocated = true;

        // get number of subdevices
        status = LwRmControl(m_hClient, m_hDev,
                             LW0080_CTRL_CMD_GPU_GET_NUM_SUBDEVICES,
                             &numSubDevicesParams,
                             sizeof (numSubDevicesParams));
        if (status != LW_OK)
        {
            Printf(Tee::PriNormal,
                   "CtrlDispTest: subdevice cnt cmd failed: 0x%x\n", status);
            return RmApiStatusToModsRC(status);
        }

        // for each subdevice...
        for (subdev = 0; subdev < numSubDevicesParams.numSubDevices; subdev++)
        {
            // ...run LW04_DISPLAY_COMMON commands test
            CHECK_RC(DispCmnTest(subdev));
        }

        // free handles allocated in this loop
        LwRmFree(m_hClient, m_hDev, m_hDisplayCmn);
        m_isDisplayCmnAllocated = false;
        LwRmFree(m_hClient, m_hClient, m_hDev);
        m_isDeviceAllocated = false;
    }

    return rc;
}

//! \brief Cleanup entry point
//!
//! Release outstanding hClient (LW01_ROOT), h_Device (LW01_DEVICE_0), and
//! h_DisplayCmn (LW04_DISPLAY_COMMON) resources.
//!
//! \return No status returned; always passes.
//-----------------------------------------------------------------------------
RC CtrlDispTest::Cleanup()
{
    if (m_isDisplayCmnAllocated)
    {
        LwRmFree(m_hClient, m_hDev, m_hDisplayCmn);
    }

    if (m_isDeviceAllocated)
    {
        LwRmFree(m_hClient, m_hClient, m_hDev);
    }

    LwRmFree(m_hClient, m_hClient, m_hClient);

    return OK;
}

//-----------------------------------------------------------------------------
// Private Member Functions
//-----------------------------------------------------------------------------

//! \brief DispCmnTest routine
//!
//! This function covers LW04_DISPLAY_COMMON control commands.
//!
//! Commands covered:
//!
//!  LW0073_CTRL_CMD_NULL
//!  LW0073_CTRL_CMD_SYSTEM_CAPS_TBL_SIZE
//!  LW0073_CTRL_CMD_SYSTEM_GET_CONECT_STATE
//!  LW0073_CTRL_CMD_SPECIFIC_GET_TYPE
//!  LW0073_CTRL_CMD_SYSTEM_GET_HEAD_ROUTING_MAP
//!  LW0073_CTRL_CMD_SYSTEM_GET_NUM_HEADS
//!  LW0073_CTRL_CMD_SYSTEM_GET_ACTIVE
//!  LW0073_CTRL_CMD_SYSTEM_GET_SCANLINE
//!  LW0073_CTRL_CMD_SYSTEM_GET_CONNECTOR_TABLE
//!
//! \return OK if the test passes, subtest-specific value indicating
//!         nature of failure otherwise
RC CtrlDispTest::DispCmnTest(UINT32 subdev)
{
    UINT32 status = 0;
    UINT32 supportedMask, connectedMask;
    RC rc;

    Printf(Tee::PriLow,
           "DispCmnTest: testing subdevice %d\n", subdev);

    // issue null command
    status = LwRmControl(m_hClient, m_hDisplayCmn, LW0073_CTRL_CMD_NULL, 0, 0);
    if (status != LW_OK)
    {
        Printf(Tee::PriNormal,
               "DispCmnTest: NULL cmd failed: 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }

    // run caps test
    CHECK_RC(DispCmnCapsTest());

    // run supported display test
    CHECK_RC(DispCmnSupportedTest(subdev, &supportedMask));

    // run connected display test
    CHECK_RC(DispCmnConnectedTest(subdev, supportedMask, &connectedMask));

    // run connected display test
    CHECK_RC(DispCmnDisplayIdTest(subdev, connectedMask));

    // run head routing test
    CHECK_RC(DispCmnHeadRoutingTest(subdev, supportedMask, connectedMask));

    // run active test
    CHECK_RC(DispCmnActiveTest(subdev));

    // run connector table test
    CHECK_RC(DispCmnConnectorTableTest(subdev));

    // try to allocate second LW04_DISPLAY_COMMON; should fail
    status = LwRmAlloc(m_hClient, m_hDev, m_hDisplayCmn + 1,
                       LW04_DISPLAY_COMMON, NULL);
    if (status != LW_ERR_STATE_IN_USE)
    {
        Printf(Tee::PriNormal,
               "DispCmnTest: LwRmAlloc test failed: status 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }

    Printf(Tee::PriLow, "DispCmnTest: passed\n");

    return rc;
}

//! \brief DispCmnCapsTest routine
//!
//! This function covers the following LW04_DISPLAY_COMMON control commands:
//!
//!  LW0073_CTRL_CMD_SYSTEM_CAPS_V2
//!
//! \return OK if the test passes, subtest-specific value indicating
//!         nature of failure otherwise
RC CtrlDispTest::DispCmnCapsTest()
{
    LW0073_CTRL_SYSTEM_GET_CAPS_V2_PARAMS dispCapsParams;
    UINT32 i, status = 0;
    RC rc;

    Printf(Tee::PriLow, "DispCmnCapsTest: testing broadcast caps cmd\n");

    // zero out param structs
    memset(&dispCapsParams, 0, sizeof (dispCapsParams));

    // get display caps (broadcast)
    status = LwRmControl(m_hClient, m_hDisplayCmn,
                         LW0073_CTRL_CMD_SYSTEM_GET_CAPS_V2,
                         &dispCapsParams, sizeof (dispCapsParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriNormal,
               "DispCmnTest: caps cmd failed: 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }

    Printf(Tee::PriLow, " broadcast display caps:\n");
    for (i = 0; i < LW0073_CTRL_SYSTEM_CAPS_TBL_SIZE; i++)
        Printf(Tee::PriLow, "  capsTbl[%d]: 0x%02x\n", i, dispCapsParams.capsTbl[i]);

    Printf(Tee::PriLow, "DispCmnCapsTest: passed\n");

    return rc;
}

//! \brief DispCmnSupportedTest routine
//!
//! This function covers the following LW04_DISPLAY_COMMON control commands:
//!
//!  LW0073_CTRL_CMD_SYSTEM_GET_SUPPORTED
//!
//! \return OK if the test passes, subtest-specific value indicating
//!         nature of failure otherwise
RC CtrlDispTest::DispCmnSupportedTest(UINT32 subdev, UINT32 *pSupportedMask)
{
    LW0073_CTRL_SYSTEM_GET_SUPPORTED_PARAMS dispSupportedParams;
    UINT32 status = 0;
    RC rc;

    Printf(Tee::PriLow,
           "DispCmnSupportedTest: testing subdevice %d\n", subdev);

    // zero out param structs
    memset(&dispSupportedParams, 0,
           sizeof (LW0073_CTRL_SYSTEM_GET_SUPPORTED_PARAMS));

    // get supported displays
    dispSupportedParams.subDeviceInstance = subdev;
    dispSupportedParams.displayMask = 0;
    status = LwRmControl(m_hClient, m_hDisplayCmn,
                         LW0073_CTRL_CMD_SYSTEM_GET_SUPPORTED,
                         &dispSupportedParams, sizeof (dispSupportedParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriNormal,
               "DispCmnSupportedTest: displays supported failed: 0x%x\n",
               status);
        return RmApiStatusToModsRC(status);
    }

    *pSupportedMask = dispSupportedParams.displayMask;

    Printf(Tee::PriLow, "  supported displays: 0x%x\n", *pSupportedMask);

    return rc;
}

//! \brief DispCmnConnectedTest routine
//!
//! This function covers the following LW04_DISPLAY_COMMON control commands:
//!
//!  LW0073_CTRL_CMD_SYSTEM_GET_CONECT_STATE
//!
//! \return OK if the test passes, subtest-specific value indicating
//!         nature of failure otherwise.
RC CtrlDispTest::DispCmnConnectedTest(UINT32 subdev, UINT32 supportedMask,
                                      UINT32 *pConnectedMask)
{
    LW0073_CTRL_SYSTEM_GET_CONNECT_STATE_PARAMS dispConnectParams;
    UINT32 status = 0;
    RC rc;

    Printf(Tee::PriLow,
           "DispCmnConnectedTest: testing subdevice %d\n", subdev);

    memset(&dispConnectParams, 0,
           sizeof (LW0073_CTRL_SYSTEM_GET_CONNECT_STATE_PARAMS));

    // get connected displays
    dispConnectParams.subDeviceInstance = subdev;
    dispConnectParams.flags = 0;
    dispConnectParams.displayMask = supportedMask;

    status = LwRmControl(m_hClient, m_hDisplayCmn,
                         LW0073_CTRL_CMD_SYSTEM_GET_CONNECT_STATE,
                         &dispConnectParams, sizeof (dispConnectParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriNormal,
               "DispCmnConnectedTest: connect state failed: 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }

    *pConnectedMask = dispConnectParams.displayMask;

    Printf(Tee::PriLow, "  connected displays: 0x%x\n", *pConnectedMask);

    return rc;
}

//! \brief DispCmnDisplayIdTest routine
//!
//! This function covers the following LW04_DISPLAY_COMMON control commands:
//!
//!  LW0073_CTRL_CMD_SPECIFIC_GET_TYPE
//!
//! \return OK if the test passes, subtest-specific value indicating
//!         nature of failure otherwise
RC CtrlDispTest::DispCmnDisplayIdTest(UINT32 subdev, UINT32 connectedMask)
{
    LW0073_CTRL_SPECIFIC_GET_TYPE_PARAMS dispTypeParams;
    UINT32 status = 0;
    UINT32 i;
    RC rc;

    Printf(Tee::PriLow,
           "DispCmnDisplayIdTest: testing subdevice %d\n", subdev);

    // zero out param structs
    memset(&dispTypeParams, 0,
           sizeof (LW0073_CTRL_SPECIFIC_GET_TYPE_PARAMS));

    // get display types
    for (i = 0; i < sizeof (connectedMask); i++)
    {
        if (((1 << i) & connectedMask) == 0)
            continue;

        // get display type
        dispTypeParams.subDeviceInstance = subdev;
        dispTypeParams.displayId = (1 << i);

        status = LwRmControl(m_hClient, m_hDisplayCmn,
                             LW0073_CTRL_CMD_SPECIFIC_GET_TYPE,
                             &dispTypeParams, sizeof (dispTypeParams));
        if (status != LW_OK)
        {
            Printf(Tee::PriNormal,
                   "DispCmnDisplayIdTest: display id failed: 0x%x\n", status);
            return RmApiStatusToModsRC(status);
        }

        Printf(Tee::PriLow, "  display id: 0x%x display type: 0x%x\n",
               (1 << i), (UINT32)dispTypeParams.displayType);
    }

    // ask for more than one display id at a time
    dispTypeParams.displayId = 0x101;
    status = LwRmControl(m_hClient, m_hDisplayCmn,
                         LW0073_CTRL_CMD_SPECIFIC_GET_TYPE,
                         &dispTypeParams, sizeof (dispTypeParams));
    if (status != LW_ERR_ILWALID_ARGUMENT)
    {
        Printf(Tee::PriNormal,
               "DispCmnDisplayIdTest: display id failed: 0x%x\n", status);
        return status;
    }

    Printf(Tee::PriLow, "DispCmnDisplayIdTest: passed\n");

    return rc;
}

//! \brief DispCmnHeadRoutingTest routine
//!
//! This function covers the following LW04_DISPLAY_COMMON control commands:
//!
//!  LW0073_CTRL_CMD_SYSTEM_GET_HEAD_ROUTING_MAP
//!
//! \return OK if the test passes, subtest-specific value indicating
//!         nature of failure otherwise
RC CtrlDispTest::DispCmnHeadRoutingTest(UINT32 subdev, UINT32 supportedMask,
                                        UINT32 connectedMask)
{
    LW0073_CTRL_SYSTEM_GET_HEAD_ROUTING_MAP_PARAMS dispHeadMapParams;
    UINT32 status = 0;
    RC rc;

    //
    // This test has to be run on real hw; simulators claim everything
    // supported is connected.
    //
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        return rc;
    }

    Printf(Tee::PriLow,
           "DispCmnHeadRoutingTest: testing subdevice %d\n", subdev);

    Printf(Tee::PriLow,
           "DispCmnHeadRoutingTest: supported 0x%x connected 0x%x\n",
           supportedMask, connectedMask);

    // this test requires connected displays
    if (connectedMask == 0)
        return rc;

    // zero out param structs
    memset(&dispHeadMapParams, 0,
           sizeof (LW0073_CTRL_SYSTEM_GET_HEAD_ROUTING_MAP_PARAMS));

    dispHeadMapParams.subDeviceInstance = subdev;
    dispHeadMapParams.displayMask = connectedMask;
    dispHeadMapParams.oldDisplayMask = 0;
    dispHeadMapParams.oldHeadRoutingMap = 0;
    dispHeadMapParams.headRoutingMap = 0;

    status = LwRmControl(m_hClient, m_hDisplayCmn,
                         LW0073_CTRL_CMD_SYSTEM_GET_HEAD_ROUTING_MAP,
                         &dispHeadMapParams, sizeof (dispHeadMapParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriNormal,
               "CtrlDispHeadRoutingTest: head map failed: 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }

    if (dispHeadMapParams.displayMask == 0)
    {
        Printf(Tee::PriLow, "  head routing map: unsupported\n");
    }
    else
    {
        Printf(Tee::PriLow, "  head routing map: 0x%x\n",
               (UINT32)dispHeadMapParams.headRoutingMap);
    }

    // send bad displayMask in for head routing assignment
    dispHeadMapParams.subDeviceInstance = subdev;
    dispHeadMapParams.displayMask = ~supportedMask;
    dispHeadMapParams.oldDisplayMask = 0;
    dispHeadMapParams.oldHeadRoutingMap = 0;
    dispHeadMapParams.headRoutingMap = 0;

    status = LwRmControl(m_hClient, m_hDisplayCmn,
                         LW0073_CTRL_CMD_SYSTEM_GET_HEAD_ROUTING_MAP,
                         &dispHeadMapParams, sizeof (dispHeadMapParams));
    if (status != LW_ERR_ILWALID_ARGUMENT)
    {
        Printf(Tee::PriNormal,
               "DispCmnHeadRoutingTest: head map failed: 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }

    Printf(Tee::PriLow, "DispCmnHeadRoutingTest: passed\n");

    return rc;
}

//! \brief DispCmnActiveTest routine
//!
//! This function covers the following LW04_DISPLAY_COMMON control commands:
//!
//!  LW0073_CTRL_CMD_SYSTEM_GET_NUM_HEADS
//!  LW0073_CTRL_CMD_SYSTEM_GET_ACTIVE
//!  LW0073_CTRL_CMD_SYSTEM_GET_SCANLINE
//!
//! \return OK if the test passes, subtest-specific value indicating
//!         nature of failure otherwise
RC CtrlDispTest::DispCmnActiveTest(UINT32 subdev)
{
    LW0073_CTRL_SYSTEM_GET_NUM_HEADS_PARAMS numHeadsParams;
    LW0073_CTRL_SYSTEM_GET_SCANLINE_PARAMS scanlineParams;
    LW0073_CTRL_SYSTEM_GET_ACTIVE_PARAMS dispActiveParams;
    UINT32 status = 0;
    UINT32 head;
    RC rc;

    Printf(Tee::PriLow,
           "DispCmnActiveTest: testing subdevice %d\n", subdev);

    // zero out param structs
    memset(&numHeadsParams, 0,
           sizeof (LW0073_CTRL_SYSTEM_GET_NUM_HEADS_PARAMS));
    memset(&scanlineParams, 0,
           sizeof (LW0073_CTRL_SYSTEM_GET_SCANLINE_PARAMS));
    memset(&dispActiveParams, 0,
           sizeof (LW0073_CTRL_SYSTEM_GET_ACTIVE_PARAMS));

    // get number of heads
    numHeadsParams.subDeviceInstance = subdev;
    numHeadsParams.flags = 0;
    status = LwRmControl(m_hClient, m_hDisplayCmn,
                         LW0073_CTRL_CMD_SYSTEM_GET_NUM_HEADS,
                         &numHeadsParams, sizeof (numHeadsParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriNormal,
               "DispCmnTest: numheads cmd failed: 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }

    // get active displays
    for (head = 0; head < numHeadsParams.numHeads; head++)
    {
        // hw active (vbios or vlcd enabled)
        dispActiveParams.subDeviceInstance = subdev;
        dispActiveParams.head = head;
        dispActiveParams.flags = 0;

        status = LwRmControl(m_hClient, m_hDisplayCmn,
                             LW0073_CTRL_CMD_SYSTEM_GET_ACTIVE,
                             &dispActiveParams, sizeof (dispActiveParams));
        if (status != LW_OK)
        {
            Printf(Tee::PriNormal,
                   "DispCmnActiveTest: get_active failed: 0x%x\n", status);
            return RmApiStatusToModsRC(status);
        }

        Printf(Tee::PriLow, "  hw active display[head=%d]: 0x%x\n",
               head, (UINT32)dispActiveParams.displayId);

        // vlcd enabled only
        dispActiveParams.flags = DRF_DEF(0073,
                                         _CTRL_SYSTEM_GET_ACTIVE_FLAGS,
                                         _CLIENT,
                                         _ENABLE);
        status = LwRmControl(m_hClient, m_hDisplayCmn,
                             LW0073_CTRL_CMD_SYSTEM_GET_ACTIVE,
                             &dispActiveParams, sizeof (dispActiveParams));
        if (status != LW_OK)
        {
            Printf(Tee::PriNormal,
                   "DispCmnActiveTest: get_active failed: 0x%x\n", status);
            return RmApiStatusToModsRC(status);
        }

        Printf(Tee::PriLow, "  client active display[head=%d]: 0x%x\n",
               head, (UINT32)dispActiveParams.displayId);

        // get current scanline value
        scanlineParams.subDeviceInstance = subdev;
        scanlineParams.head = head;
        status = LwRmControl(m_hClient, m_hDisplayCmn,
                             LW0073_CTRL_CMD_SYSTEM_GET_SCANLINE,
                             &scanlineParams, sizeof (scanlineParams));
        if (status != LW_OK)
        {
            Printf(Tee::PriNormal,
                   "DispCmnActiveTest: scanline cmd failed: 0x%x\n", status);
            return RmApiStatusToModsRC(status);
        }

        Printf(Tee::PriLow, "  scanline[head=%d]: 0x%x\n",
               head, (UINT32)scanlineParams.lwrrentScanline);
    }

    // test bad head to current scanline cmd
    scanlineParams.subDeviceInstance = subdev;
    scanlineParams.head = 0xffff;
    status = LwRmControl(m_hClient, m_hDisplayCmn,
                         LW0073_CTRL_CMD_SYSTEM_GET_SCANLINE,
                         &scanlineParams, sizeof (scanlineParams));
    if (status != LW_ERR_ILWALID_ARGUMENT)
    {
        Printf(Tee::PriNormal,
               "DispCmnActiveTest: scanline failed: expected 0x%x, got 0x%x\n",
               LW_ERR_ILWALID_ARGUMENT, status);
        return RmApiStatusToModsRC(status);
    }

    Printf(Tee::PriLow, "DispCmnActiveTest: passed\n");

    return rc;
}

//! \brief DispCmnGetConnectorTable routine
//!
//! This function covers the following LW04_DISPLAY_COMMON control commands:
//!
//!  LW0073_CTRL_CMD_SYSTEM_GET_CONNECTOR_TABLE
//!
//! \return OK if the test passes, subtest-specific value indicating
//!         nature of failure otherwise
RC CtrlDispTest::DispCmnConnectorTableTest(UINT32 subdev)
{
    LW0073_CTRL_SYSTEM_GET_CONNECTOR_TABLE_PARAMS getConnectorTableParams;
    UINT32 status = 0;
    UINT32 i;
    RC rc;

    Printf(Tee::PriLow,
           "DispCmnConnectorTableTest: testing subdevice %d\n", subdev);

    // zero out param structs
    memset(&getConnectorTableParams, 0,
           sizeof (LW0073_CTRL_SYSTEM_GET_CONNECTOR_TABLE_PARAMS));

    // get connector table
    getConnectorTableParams.subDeviceInstance = subdev;
    status = LwRmControl(m_hClient, m_hDisplayCmn,
                         LW0073_CTRL_CMD_SYSTEM_GET_CONNECTOR_TABLE,
                         &getConnectorTableParams,
                         sizeof (getConnectorTableParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriNormal,
               "DispCmnGetConnectorTableTest: cmd failed: 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }

    Printf(Tee::PriLow, "  version=0x%x platform=0x%x numEntries=0x%x\n",
           getConnectorTableParams.version,
           getConnectorTableParams.platform,
           getConnectorTableParams.connectorTableEntries);

    for (i = 0; i < getConnectorTableParams.connectorTableEntries; i++)
    {
        Printf(Tee::PriLow, "    connector[%d]:\n", i);

        Printf(Tee::PriLow,
               "      type=0x%x displayMask=0x%x loc=0x%x hotplug=0x%x\n",
               getConnectorTableParams.connectorTable[i].type,
               getConnectorTableParams.connectorTable[i].displayMask,
               getConnectorTableParams.connectorTable[i].location,
               getConnectorTableParams.connectorTable[i].hotplug);
    }

    Printf(Tee::PriLow, "DispCmnConnectorTableTest: passed\n");

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
JS_CLASS_INHERIT(CtrlDispTest, RmTest,
                 "CtrlDispTest RM Test.");
