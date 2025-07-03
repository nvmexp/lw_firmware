/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_detectedid.cpp
//! \brief Test for running detection and EDID calls for all displayIds.
//!
//! This test calls the RM to test various ways of detection and it will
//! atttempt to read the EDID on all displayIDs.  The reason for the test
//! is just to exercise more parts of the detection and edid code rather
//! than looking for an actual failure from the test itself.  Also, the
//! code for decoding the EDID will be placed in here as well close
//! to the lwedid utility.

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "lwRmApi.h"
#include "rmt_detectedid.h"
#include "lwmisc.h"

#include "class/cl0073.h"
#include "ctrl/ctrl0000.h"
#include "ctrl/ctrl0073.h"
#include "ctrl/ctrl0080.h"
#include "core/include/memcheck.h"

class DetectEdidTest : public RmTest
{
public:
    DetectEdidTest();
    virtual ~DetectEdidTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    //@{
    SETGET_PROP(RunMode, UINT32);    //!< Function to grab JS variable RunMode
    SETGET_PROP(UserDevice, UINT32); //!< Function to grab JS variable UserDevice,
    SETGET_PROP(UserSub, UINT32);    //!< Function to grab JS variable UserSub
    SETGET_PROP(DisplayMask, UINT32);//!< Function to grab JS variable DisplayMask
    SETGET_PROP(CtrlFlags, UINT32);  //!< Function to grab JS variable CtrlFlags
    //@}

private:
    void PrintDTB(UINT32, UINT08 *);
    void PrintCEABlock1(UINT08 *);
    void PrintCEABlock2(UINT08 *);
    void PrintCEABlock3(UINT08 *);
    void PrintCEABlock(UINT08 *);
    void PrintDI_EXTBlock(UINT08 *);
    void PrintEdid1(UINT08 *);
    void PrintEdid2(UINT08 *);
    void DescribeEdid(UINT08 *);
    RC DetectTest(UINT32 subdev);
    RC EdidTest(UINT32 subdev);

    // Run mode type
    #define LW_RUN_MODE_TYPE_DEFAULT                     (0x00000000)
    #define LW_RUN_MODE_TYPE_DETECT                      (0x00000001)
    #define LW_RUN_MODE_TYPE_EDID                        (0x00000002)
    //  0 = Normal run
    //  1 = Detect one gpu's displays
    //  2 = Grab an EDID from a GPU and if found, describe it.
    UINT32 m_RunMode;    //!< Current mode of operation during Run() command
    // User device
    UINT32 m_UserDevice; //!< User suggested device
    // User subdevice
    UINT32 m_UserSub;    //!< User suggested subdevice
    // DisplayMask
    //  For MODE_TYPE_DETECT, this will be the mask of displays to detect.
    //    If 0, we'll detect all possible displays.
    //  For MODE_TYPE_EDID, this will be the mask of displays to read the
    //    edid.  If 0, we'll attempt EDID read on all possible displays.
    UINT32 m_DisplayMask;//!< User suggested displayMask
    // CtrlFlags
    //   These flags are passed directly into the RMCtrl call if non-zero.
    UINT32 m_CtrlFlags;  //!< User suggested control flags

    //
    // object handles
    //
    #define LW_DETECT_EDID_DEVICE_HANDLE                (0xde008000)
    #define LW_DETECT_EDID_DISPLAY_HANDLE               (0xde007300)

    LwRm::Handle m_hClient; //!< Handle to the RM Client
    static const LwRm::Handle m_hDevice = LW_DETECT_EDID_DEVICE_HANDLE; //!< Base handle to the RM Device
    static const LwRm::Handle m_hDisplay = LW_DETECT_EDID_DISPLAY_HANDLE; //!< Handle to the RM Common Display object
    UINT32 m_hDev; //!< Lwrrently allocated handle to the RM Device

    bool m_isDeviceAllocated; //!< tells us if the device is lwrrently allocated
    bool m_isDisplayAllocated; //!< tells us if the common display object is lwrrently allocated

    UINT08 *m_EdidBuffer; //!< Holds DDC read edid used in EdidTest.  Allocated in Setup, Freed in Cleanup.
    UINT08 *m_CachedBuffer; //!< Holds DDC read edid used in EdidTest. Allocated in Setup, Freed in Cleanup.

};

//! \brief DetectEdidTest basic constructor
//!
//! This is just the basic constructor for the DetectEdidTest class.
//!
//! \sa Setup
//------------------------------------------------------------------------------
DetectEdidTest::DetectEdidTest()
{
    SetName("DetectEdidTest");
    m_RunMode      = 0;
    m_UserDevice   = 0;
    m_UserSub      = 0;
    m_DisplayMask  = 0;
    m_CtrlFlags    = 0;
    m_EdidBuffer   = NULL;
    m_CachedBuffer = NULL;

}

//! \brief DetectEdidTest destructor
//!
//! This is just the destructor for the DetectEdidTest class.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
DetectEdidTest::~DetectEdidTest()
{

}

//! \brief IsSupported : test for support on current environment
//!
//! This test is mainly for real HW only.  It probably could be tested on
//! emulation, but for now, I'm just going to limit it to actual HW.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string DetectEdidTest::IsTestSupported()
{
    if( Platform::GetSimulationMode() == Platform::Hardware )
        return RUN_RMTEST_TRUE;
    return "Supported only on hardware";
}

//! \brief Setup Function, used for the test related allocations
//!
//! Nothing to do here for Setup
//
//! \return Always passes
//------------------------------------------------------------------------------
RC DetectEdidTest::Setup()
{
    RC rc;
    UINT32 status;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    m_hClient = 0;
    status = (UINT32)LwRmAllocRoot((LwU32*)&m_hClient);
    rc = RmApiStatusToModsRC(status);
    CHECK_RC(rc);

    m_EdidBuffer = (UINT08*)new UINT08[1024];
    if (!m_EdidBuffer)
    {
        Printf(Tee::PriNormal,
            "DetectEdidTest: Edid buffer memory allocation failed\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    m_CachedBuffer = (UINT08*)new UINT08[1024];
    if (!m_CachedBuffer)
    {
        Printf(Tee::PriNormal,
            "DetectEdidTest: Cached buffer memory allocation failed\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    return rc;
}

//! \brief The actual code to run the DetectEdidTest test
//!
//! This test will:
//!  A. run detection over all displays using all methods possible
//!  B. run edid calls over all displays
//!  C. decode any edid information found from B.
//!
//! \return Always passes
//------------------------------------------------------------------------------
RC DetectEdidTest::Run()
{
    RC rc = OK;
    LW0000_CTRL_GPU_GET_ATTACHED_IDS_PARAMS gpuAttachedIdsParams = {{0}};
    LW0000_CTRL_GPU_GET_ID_INFO_PARAMS gpuIdInfoParams = {0};
    LW0080_ALLOC_PARAMETERS lw0080Params = {0};
    LW0080_CTRL_GPU_GET_NUM_SUBDEVICES_PARAMS numSubDevicesParams = {0};
    UINT32 status = 0;
    UINT32 i;
    UINT32 maxDevice = 0;

    if (!m_hClient)
    {
        return (RC::DID_NOT_INITIALIZE_RM);
    }

    // get attached gpus ids
    status = LwRmControl(m_hClient, m_hClient,
                         LW0000_CTRL_CMD_GPU_GET_ATTACHED_IDS,
                         &gpuAttachedIdsParams, sizeof (gpuAttachedIdsParams));

    if (status != LW_OK)
    {
        Printf(Tee::PriNormal,
            "DetectEdidTest: GET_IDS failed: 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }

    // for each attached gpu...
    gpuIdInfoParams.szName = (LwP64)NULL;
    for (i = 0; i < LW0000_CTRL_GPU_MAX_ATTACHED_GPUS; i++)
    {
        if (gpuAttachedIdsParams.gpuIds[i] == 0xffffffff)
        {
            break;
        }

        // get gpu instance info
        gpuIdInfoParams.gpuId = gpuAttachedIdsParams.gpuIds[i];
        status = LwRmControl(m_hClient, m_hClient,
                             LW0000_CTRL_CMD_GPU_GET_ID_INFO,
                             &gpuIdInfoParams, sizeof (gpuIdInfoParams));

        if (status != LW_OK)
        {
            Printf(Tee::PriNormal,
                "DetectEdidTest: GET_ID_INFO failed: 0x%x\n", status);
            return RmApiStatusToModsRC(status);
        }

        if (maxDevice <= (UINT32)gpuIdInfoParams.deviceInstance)
        {
            maxDevice = 1+(UINT32)gpuIdInfoParams.deviceInstance;
        }

        if (m_RunMode != LW_RUN_MODE_TYPE_DEFAULT)
        {
            // Only run the test on the device requested
            if (m_UserDevice != (UINT32)gpuIdInfoParams.deviceInstance)
            {
                continue;
            }
        }

        //
        // allocate the gpu's device.
        // use LwRmAlloc to bypass display driver escape
        //
        lw0080Params.deviceId = gpuIdInfoParams.deviceInstance;
        lw0080Params.hClientShare = 0;
        m_hDev = m_hDevice + gpuIdInfoParams.deviceInstance;
        status = LwRmAlloc(m_hClient, m_hClient, m_hDev,
                           LW01_DEVICE_0,
                           &lw0080Params);
        if (status != LW_OK)
        {
            Printf(Tee::PriNormal,
                   "DetectEdidTest: LwRmAlloc(LW01_DEVICE_0) failed!\n");
            return RmApiStatusToModsRC(status);
        }

        m_isDeviceAllocated = true;

        // allocate LW04_DISPLAY_COMMON
        status = LwRmAlloc(m_hClient, m_hDev, m_hDisplay, LW04_DISPLAY_COMMON, NULL);
        if (status != LW_OK)
        {
            Printf(Tee::PriNormal,
                   "DetectEdidTest: LwRmAlloc(LW04_DISPLAY_COMMON) failed: "
                   "status 0x%x\n", status);
            return RmApiStatusToModsRC(status);
        }

        m_isDisplayAllocated = true;

        if (m_RunMode != LW_RUN_MODE_TYPE_DEFAULT)
        {
            // get number of subdevices
            status = LwRmControl(m_hClient, m_hDev,
                                 LW0080_CTRL_CMD_GPU_GET_NUM_SUBDEVICES,
                                 &numSubDevicesParams,
                                 sizeof (numSubDevicesParams));
            if (status != LW_OK)
            {
                Printf(Tee::PriNormal,
                    "DetectEdidTest: subdevice cnt cmd failed: 0x%x\n", status);
                return RmApiStatusToModsRC(status);
            }

            if (m_UserSub >= numSubDevicesParams.numSubDevices)
            {
                Printf(Tee::PriNormal,
                    "DetectEdidTest: Requested Subdevice %d must be less than"
                    " max subdevices %d!\n", m_UserSub,
                    (UINT32)numSubDevicesParams.numSubDevices);
                return RC::BAD_PARAMETER;
            }
        }

        // do unicast (subdevice) commands
        switch (m_RunMode)
        {
            default:
            case LW_RUN_MODE_TYPE_DEFAULT:
                Printf(Tee::PriNormal,
                    "DetectEdidTest: Running on device %d, subdevice %d:\n",
                    (UINT32)gpuIdInfoParams.deviceInstance,
                    (UINT32)gpuIdInfoParams.subDeviceInstance);
                CHECK_RC(DetectTest((UINT32)gpuIdInfoParams.subDeviceInstance));
                CHECK_RC(EdidTest((UINT32)gpuIdInfoParams.subDeviceInstance));
                break;
            case LW_RUN_MODE_TYPE_DETECT:
                if (m_UserSub == (UINT32)gpuIdInfoParams.subDeviceInstance)
                {
                    Printf(Tee::PriNormal,
                        "DetectEdidTest: Running on device %d, subdevice %d:\n",
                        (UINT32)gpuIdInfoParams.deviceInstance,
                        (UINT32)gpuIdInfoParams.subDeviceInstance);
                    CHECK_RC(DetectTest((UINT32)gpuIdInfoParams.subDeviceInstance));
                }
                break;
            case LW_RUN_MODE_TYPE_EDID:
                if (m_UserSub == (UINT32)gpuIdInfoParams.subDeviceInstance)
                {
                    Printf(Tee::PriNormal,
                        "DetectEdidTest: Running on device %d, subdevice %d:\n",
                        (UINT32)gpuIdInfoParams.deviceInstance,
                        (UINT32)gpuIdInfoParams.subDeviceInstance);
                    CHECK_RC(EdidTest((UINT32)gpuIdInfoParams.subDeviceInstance));
                }
                break;
        }

        // free handles allocated in this loop
        LwRmFree(m_hClient, m_hDev, m_hDisplay);
        m_isDisplayAllocated = false;
        LwRmFree(m_hClient, m_hClient, m_hDev);
        m_isDeviceAllocated = false;
    }

    if (m_RunMode != LW_RUN_MODE_TYPE_DEFAULT)
    {
        if (m_UserDevice >= maxDevice)
        {
            Printf(Tee::PriNormal,
                "DetectEdidTest: Requested Device %d must be less than"
                " max device %d!\n", m_UserDevice, maxDevice);
            return RC::BAD_PARAMETER;
        }
    }

    return rc;
}

//! \brief Cleanup Function
//!
//! Cleanup all allocated data during Setup().
//------------------------------------------------------------------------------
RC DetectEdidTest::Cleanup()
{
    if (m_CachedBuffer)
    {
        delete [] m_CachedBuffer;
        m_CachedBuffer = NULL;
    }
    if (m_EdidBuffer)
    {
        delete [] m_EdidBuffer;
        m_EdidBuffer = NULL;
    }

    if (m_isDisplayAllocated)
    {
        LwRmFree(m_hClient, m_hDev, m_hDisplay);
    }

    if (m_isDeviceAllocated)
    {
        LwRmFree(m_hClient, m_hClient, m_hDev);
    }

    LwRmFree(m_hClient, m_hClient, m_hClient);
    return OK;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------
//! \brief Detect runs all permutations of detect known
//!
//! This function takes a subdevice argument and runs detection on that one
//! subdevice.  It should run a normal detection. Then it will ask for the
//! cached detect state. Then it runs with DDC disabled, Load detect disabled,
//! and then both disabled. Finally it runs Brief DDC detection. On each run,
//! it should print out the results.
RC DetectEdidTest::DetectTest(UINT32 subdev)
{
    LW0073_CTRL_SYSTEM_GET_SUPPORTED_PARAMS dispSupportedParams = {0};
    LW0073_CTRL_SYSTEM_GET_CONNECT_STATE_PARAMS dispConnectParams = {0};
    UINT32 status = 0;

    // get supported displays
    dispSupportedParams.subDeviceInstance = subdev;
    dispSupportedParams.displayMask = 0;
    status = LwRmControl(m_hClient, m_hDisplay,
                         LW0073_CTRL_CMD_SYSTEM_GET_SUPPORTED,
                         &dispSupportedParams, sizeof (dispSupportedParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriNormal,
            "DetectEdidTest: displays supported failed: 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }

    Printf(Tee::PriNormal,
           "  Supported displays: 0x%x\n",
           (UINT32)dispSupportedParams.displayMask);

    // get connected displays
    dispConnectParams.subDeviceInstance = subdev;
    dispConnectParams.flags = 0;
    dispConnectParams.flags =
        FLD_SET_DRF(0073_CTRL_SYSTEM, _GET_CONNECT_STATE_FLAGS,
                    _METHOD, _DEFAULT, dispConnectParams.flags);
    dispConnectParams.flags =
        FLD_SET_DRF(0073_CTRL_SYSTEM, _GET_CONNECT_STATE_FLAGS,
                    _DDC,    _DEFAULT, dispConnectParams.flags);
    dispConnectParams.flags =
        FLD_SET_DRF(0073_CTRL_SYSTEM, _GET_CONNECT_STATE_FLAGS,
                    _LOAD,   _DEFAULT, dispConnectParams.flags);

    if (m_DisplayMask)
    {
        if (!((m_DisplayMask &
                dispSupportedParams.displayMask) == m_DisplayMask) )
        {
            Printf(Tee::PriNormal,
                "DetectEdidTest: DisplayMask 0x%x must be subset of\n"
                "                supported displays, 0x%x!\n",
                m_DisplayMask, (UINT32)dispSupportedParams.displayMask);
            return RC::BAD_PARAMETER;
        }
        dispConnectParams.displayMask = m_DisplayMask;
    }
    else
    {
        dispConnectParams.displayMask = dispSupportedParams.displayMask;
    }

    // setup the flags from the user if this is not the default run.
    if (m_RunMode != LW_RUN_MODE_TYPE_DEFAULT)
    {
        dispConnectParams.flags = m_CtrlFlags;
    }

    status = LwRmControl(m_hClient, m_hDisplay,
                         LW0073_CTRL_CMD_SYSTEM_GET_CONNECT_STATE,
                         &dispConnectParams, sizeof (dispConnectParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriNormal,
            "DetectEdidTest: Default connect state failed: 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }

    if (m_RunMode == LW_RUN_MODE_TYPE_DEFAULT)
    {
        Printf(Tee::PriNormal,
               "  Complete detect reveals connected displays: 0x%x\n",
               (UINT32)dispConnectParams.displayMask);
    }
    else
    {
        if ((m_CtrlFlags & 0x3) == 0x1)
        {
            Printf(Tee::PriNormal, "  Cached detect of ");
        }
        else if ((m_CtrlFlags & 0x3) == 0x2)
        {
            Printf(Tee::PriNormal, "  Brief DDC detect of ");
        }
        else
        {
            switch (m_CtrlFlags & 0x30)
            {
                default:
                case 0x00:
                    Printf(Tee::PriNormal, "  Complete detect of ");
                    break;
                case 0x10:
                    Printf(Tee::PriNormal, "  DDC disabled detect of ");
                    break;
                case 0x20:
                    Printf(Tee::PriNormal, "  Load disabled detect of ");
                    break;
                case 0x30:
                    Printf(Tee::PriNormal,
                        "  DDC and Load disabled detect of ");
                    break;
            }
        }
        if (m_DisplayMask)
        {
            Printf(Tee::PriNormal, "0x%x ", m_DisplayMask);
        }
        else
        {
            Printf(Tee::PriNormal, "0x%x ",
                (UINT32)dispSupportedParams.displayMask);
        }
        Printf(Tee::PriNormal, "shows connected displays: 0x%x\n",
            (UINT32)dispConnectParams.displayMask);

        // nothing else to do here....
        return OK;
    }

    dispConnectParams.flags =
        FLD_SET_DRF(0073_CTRL_SYSTEM, _GET_CONNECT_STATE_FLAGS,
                    _METHOD, _CACHED, dispConnectParams.flags);
    status = LwRmControl(m_hClient, m_hDisplay,
                         LW0073_CTRL_CMD_SYSTEM_GET_CONNECT_STATE,
                         &dispConnectParams, sizeof (dispConnectParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriNormal,
            "DetectEdidTest: Cached connect state failed: 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }

    Printf(Tee::PriNormal,
           "  Cached detect reveals connected displays: 0x%x\n",
           (UINT32)dispConnectParams.displayMask);

    dispConnectParams.flags =
        FLD_SET_DRF(0073_CTRL_SYSTEM, _GET_CONNECT_STATE_FLAGS,
                    _METHOD, _ECONODDC, dispConnectParams.flags);
    status = LwRmControl(m_hClient, m_hDisplay,
                         LW0073_CTRL_CMD_SYSTEM_GET_CONNECT_STATE,
                         &dispConnectParams, sizeof (dispConnectParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriNormal,
            "DetectEdidTest: Econo DDC connect state failed: 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }

    Printf(Tee::PriNormal,
           "  Econo DDC detect reveals connected displays: 0x%x\n",
           (UINT32)dispConnectParams.displayMask);

    dispConnectParams.flags =
        FLD_SET_DRF(0073_CTRL_SYSTEM, _GET_CONNECT_STATE_FLAGS,
                    _METHOD, _DEFAULT, dispConnectParams.flags);
    dispConnectParams.flags =
        FLD_SET_DRF(0073_CTRL_SYSTEM, _GET_CONNECT_STATE_FLAGS,
                    _DDC,    _DISABLE, dispConnectParams.flags);
    dispConnectParams.flags =
        FLD_SET_DRF(0073_CTRL_SYSTEM, _GET_CONNECT_STATE_FLAGS,
                    _LOAD,   _DEFAULT, dispConnectParams.flags);

    status = LwRmControl(m_hClient, m_hDisplay,
                         LW0073_CTRL_CMD_SYSTEM_GET_CONNECT_STATE,
                         &dispConnectParams, sizeof (dispConnectParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriNormal,
            "DetectEdidTest: DDC Disabled connect state failed: 0x%x\n",
            status);
        return RmApiStatusToModsRC(status);
    }

    Printf(Tee::PriNormal,
           "  DDC Disabled detect reveals connected displays: 0x%x\n",
           (UINT32)dispConnectParams.displayMask);

    dispConnectParams.flags =
        FLD_SET_DRF(0073_CTRL_SYSTEM, _GET_CONNECT_STATE_FLAGS,
                    _METHOD, _DEFAULT, dispConnectParams.flags);
    dispConnectParams.flags =
        FLD_SET_DRF(0073_CTRL_SYSTEM, _GET_CONNECT_STATE_FLAGS,
                    _DDC,    _DEFAULT, dispConnectParams.flags);
    dispConnectParams.flags =
        FLD_SET_DRF(0073_CTRL_SYSTEM, _GET_CONNECT_STATE_FLAGS,
                    _LOAD,   _DISABLE, dispConnectParams.flags);

    status = LwRmControl(m_hClient, m_hDisplay,
                         LW0073_CTRL_CMD_SYSTEM_GET_CONNECT_STATE,
                         &dispConnectParams, sizeof (dispConnectParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriNormal,
            "DetectEdidTest: Load Disabled connect state failed: 0x%x\n",
            status);
        return RmApiStatusToModsRC(status);
    }

    Printf(Tee::PriNormal,
           "  Load Disabled detect reveals connected displays: 0x%x\n",
           (UINT32)dispConnectParams.displayMask);

    dispConnectParams.flags =
        FLD_SET_DRF(0073_CTRL_SYSTEM, _GET_CONNECT_STATE_FLAGS,
                    _METHOD, _DEFAULT, dispConnectParams.flags);
    dispConnectParams.flags =
        FLD_SET_DRF(0073_CTRL_SYSTEM, _GET_CONNECT_STATE_FLAGS,
                    _DDC,    _DISABLE, dispConnectParams.flags);
    dispConnectParams.flags =
        FLD_SET_DRF(0073_CTRL_SYSTEM, _GET_CONNECT_STATE_FLAGS,
                    _LOAD,   _DISABLE, dispConnectParams.flags);

    status = LwRmControl(m_hClient, m_hDisplay,
                         LW0073_CTRL_CMD_SYSTEM_GET_CONNECT_STATE,
                         &dispConnectParams, sizeof (dispConnectParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriNormal,
            "DetectEdidTest: Load and DDC Disabled "
            "connect state failed: 0x%x\n",
            status);
        return RmApiStatusToModsRC(status);
    }

    Printf(Tee::PriNormal,
           "  Load and DDC Disabled detect reveals connected displays: 0x%x\n",
           (UINT32)dispConnectParams.displayMask);

    return OK;
}

//! \brief Describes one Detailed Timing Block from the EDID
//!
//! This function takes a pointer to a byte buffer and then describes the
//! Detailed Timing block.
void DetectEdidTest::PrintDTB(UINT32 i, UINT08 *dtb)
{
    DTB    *dtd = (DTB *)dtb;
    UINT32  j;
    // Left or right column marker
    UINT08   left;
    UINT16  h, v;
    UINT32  hratios[] = { 16, 4, 5, 16 };
    UINT32  vratios[] = { 10, 3, 4,  9 };
    UINT16  pclock = (((UINT16)dtd->pclocklo)   ) |
                     (((UINT16)dtd->pclockhi)<<8);

    if (pclock != 0x0000)
    {
        UINT32 HActive =
            (((UINT32)DRF_VAL(_EDID1, _HACTBLANK,
                              _HACTIVEHI, dtd->hactblank)) << 8) |
            ((UINT32)dtd->hactive);
        UINT32 HBlank =
            (((UINT32)DRF_VAL(_EDID1, _HACTBLANK,
                              _HBLANKHI, dtd->hactblank)) << 8) |
            ((UINT32)dtd->hblank);
        UINT32 VActive =
            (((UINT32)DRF_VAL(_EDID1, _VACTBLANK,
                              _VACTIVEHI, dtd->vactblank)) << 8) |
             ((UINT32)dtd->vactive);
        UINT32 VBlank =
            (((UINT32)DRF_VAL(_EDID1, _VACTBLANK,
                             _VBLANKHI, dtd->vactblank)) << 8) |
             ((UINT32)dtd->vblank);
        UINT32 HTotal = HActive + HBlank;
        UINT32 VTotal = VActive + VBlank;
        UINT32 HSyncOffset =
            (((UINT32)DRF_VAL(_EDID1, _SYNCCOMBO,
                              _HS_OFF_HI, dtd->SyncCombo)) << 8) |
             ((UINT32)dtd->HSOffset);
        UINT32 HSyncWidth =
            (((UINT32)DRF_VAL(_EDID1, _SYNCCOMBO,
                              _HS_PW_HI, dtd->SyncCombo)) << 8) |
             ((UINT32)dtd->HSPulseWidth);
        UINT32 VSyncOffset =
            (((UINT32)DRF_VAL(_EDID1, _SYNCCOMBO,
                              _VS_OFF_HI, dtd->SyncCombo)) << 4) |
             ((UINT32)DRF_VAL(_EDID1, _VSPWOFF,
                              _VS_OFFLO, dtd->VSPwOff));
        UINT32 VSyncWidth =
            (((UINT32)DRF_VAL(_EDID1, _SYNCCOMBO,
                              _VS_PW_HI, dtd->SyncCombo)) << 4) |
             ((UINT32)DRF_VAL(_EDID1, _VSPWOFF,
                              _VS_PWLO, dtd->VSPwOff));
        UINT32 HImageSize =
            (((UINT32)DRF_VAL(_EDID1, _IMAGESIZEHI,
                            _H, dtd->ImageSizeHi)) << 8) |
             ((UINT32)dtd->hImageSize);
        UINT32 VImageSize =
            (((UINT32)DRF_VAL(_EDID1, _IMAGESIZEHI,
                            _V, dtd->ImageSizeHi)) << 8) |
             ((UINT32)dtd->vImageSize);

        float PixClk = (pclock * 10000.0f) / (HTotal * VTotal);

        if (DRF_VAL(_EDID1, _FLAGS, _INTERLACED, dtd->Flags))
        {
            PixClk = ((pclock * 10000.0f) * 2.0f) /
                     (HTotal * (2*VTotal+1));
        }

        Printf(Tee::PriNormal,
            "DTD%d : %d x %d     Callwlated Refresh Rate: %3.2f Hz", i,
            HActive,
            VActive,
            PixClk);

        {
            Printf(Tee::PriNormal, "\n");
            Printf(Tee::PriNormal,
                "  Pixel Clock       [%04X]: %3.2f Mhz\n", pclock,
                    (float)(pclock/100.0));
            Printf(Tee::PriNormal,
                "  HActive           [%04X]: %4d pixels\n",
                HActive, HActive);
            Printf(Tee::PriNormal,
                "  HBlank            [%04X]: %4d pixels\n",
                HBlank, HBlank);
            Printf(Tee::PriNormal,
                "  HSync Offset      [%04X]: %4d pixels\n",
                HSyncOffset, HSyncOffset);
            Printf(Tee::PriNormal,
                "  HSync Pulse Width [%04X]: %4d pixels\n",
                HSyncWidth, HSyncWidth);

            Printf(Tee::PriNormal,
                "  VActive           [%04X]: %4d lines\n",
                VActive, VActive);
            Printf(Tee::PriNormal,
                "  VBlank            [%04X]: %4d lines\n",
                VBlank, VBlank);
            Printf(Tee::PriNormal,
                "  VSync Offset      [%04X]: %4d lines\n",
                VSyncOffset, VSyncOffset);
            Printf(Tee::PriNormal,
                "  VSync Pulse Width [%04X]: %4d lines\n",
                VSyncWidth, VSyncWidth);

            Printf(Tee::PriNormal,
                "  Image Size   [%04X %04X]: %d x %d cm\n",
                HImageSize, VImageSize,
                HImageSize, VImageSize);

            Printf(Tee::PriNormal, "  Flags                   :");
            if (DRF_VAL(_EDID1, _FLAGS, _INTERLACED, dtd->Flags))
            {
                Printf(Tee::PriNormal, " Interlaced\n");
            }
            else
            {
                Printf(Tee::PriNormal, " Non-interlaced\n");
            }
            switch(DRF_VAL(_EDID1, _FLAGS, _SYNC_CONFIG, dtd->Flags))
            {
                case 0:
                    Printf(Tee::PriNormal, "  Analog Composite        :");
                    if (DRF_VAL(_EDID1, _FLAGS, _VPOLARITY, dtd->Flags))
                    {
                        Printf(Tee::PriNormal, " Hsync during Vsync,");
                    }
                    else
                    {
                        Printf(Tee::PriNormal, " No Hsync during Vsync,");
                    }

                    if (DRF_VAL(_EDID1, _FLAGS, _HPOLARITY, dtd->Flags))
                    {
                        Printf(Tee::PriNormal, " Sync on RGB\n");
                    }
                    else
                    {
                        Printf(Tee::PriNormal, " Sync on Green\n");
                    }
                    break;
                case 1:
                    Printf(Tee::PriNormal, "  Bipolar Analog Composite:");
                    if (DRF_VAL(_EDID1, _FLAGS, _VPOLARITY, dtd->Flags))
                    {
                        Printf(Tee::PriNormal, " Hsync during Vsync,");
                    }
                    else
                    {
                        Printf(Tee::PriNormal, " No Hsync during Vsync,");
                    }

                    if (DRF_VAL(_EDID1, _FLAGS, _HPOLARITY, dtd->Flags))
                    {
                        Printf(Tee::PriNormal, " Sync on RGB\n");
                    }
                    else
                    {
                        Printf(Tee::PriNormal, " Sync on Green\n");
                    }
                    break;
                case 2:
                    Printf(Tee::PriNormal, "  Digital Composite       :");
                    if (DRF_VAL(_EDID1, _FLAGS, _VPOLARITY, dtd->Flags))
                    {
                        Printf(Tee::PriNormal, " Hsync during Vsync,");
                    }
                    else
                    {
                        Printf(Tee::PriNormal, " No Hsync during Vsync,");
                    }

                    if (DRF_VAL(_EDID1, _FLAGS, _HPOLARITY, dtd->Flags))
                    {
                        Printf(Tee::PriNormal,
                            " HSync outside Vsync Polarity:+\n");
                    }
                    else
                    {
                        Printf(Tee::PriNormal,
                            " HSync outside Vsync Polarity:-\n");
                    }
                    break;
                case 3:
                    Printf(Tee::PriNormal, "  Digital Separate        :");
                    Printf(Tee::PriNormal, " HSync/VSync polarity: %c/%c\n",
                           (DRF_VAL(_EDID1, _FLAGS,
                                    _HPOLARITY, dtd->Flags) ? '+' : '-'),
                           (DRF_VAL(_EDID1, _FLAGS,
                                    _VPOLARITY, dtd->Flags) ? '+' : '-'));
                    break;
            }
            if (DRF_VAL(_EDID1, _FLAGS, _STEREO_MODE, dtd->Flags))
            {
                Printf(Tee::PriNormal, "  Stereo                  : ");
                switch((DRF_VAL(_EDID1, _FLAGS,
                                _STEREO_MODE,        dtd->Flags)<<1) |
                       (DRF_VAL(_EDID1, _FLAGS,
                                _STEREO_INTERLEAVED, dtd->Flags)   ))
                {
                    default:
                        Printf(Tee::PriNormal,
                           "?? BIT6:%d  BIT5:%d  BIT0: %d ??\n",
                           (DRF_VAL(_EDID1, _FLAGS,
                                     _STEREO_MODE, dtd->Flags)>>1),
                           (DRF_VAL(_EDID1, _FLAGS,
                                     _STEREO_MODE, dtd->Flags)&0x1),
                           (DRF_VAL(_EDID1, _FLAGS,
                                _STEREO_INTERLEAVED, dtd->Flags)));
                        break;
                    case 2:
                        Printf(Tee::PriNormal,
                            "Field sequential stereo, "
                            "right image when stereo sync=1\n");
                            break;
                    case 4:
                        Printf(Tee::PriNormal,
                            "Field sequential stereo, "
                            "left image when stereo sync=1\n");
                            break;
                    case 3:
                        Printf(Tee::PriNormal,
                            "2-way interleaved stereo, "
                            "right image on even lines\n");
                        break;
                    case 5:
                        Printf(Tee::PriNormal,
                            "2-way interleaved stereo, "
                            "left image on even lines\n");
                        break;
                    case 6:
                        Printf(Tee::PriNormal,
                            "4-way interleaved stereo\n");
                        break;
                    case 7:
                        Printf(Tee::PriNormal,
                            "Side-by-Side interleaved stereo\n");
                        break;
                }
            }

        }
    }
    else
    {
        // pclock == 0
        DTDB *dtdb = (DTDB *)dtb;
        char asciistr[14];
        // Check to make sure that all flags are 0
        if (!(dtdb->Flag0Lo) &&
            !(dtdb->Flag0Hi) &&
            !(dtdb->Flag1  ) &&
            !(dtdb->Flag2  ) )
        {
            switch(dtdb->DataTypeTag)
            {
                case DTT_MONITOR_SERIAL_NUMBER:
                    Printf(Tee::PriNormal,
                        "DTD%d : Monitor Serial Number: ", i);
                    break;
                case DTT_ASCII_STRING:
                    Printf(Tee::PriNormal,
                        "DTD%d : ASCII String: ", i);
                    break;
                case DTT_MONITOR_RANGE_LIMITS:
                    Printf(Tee::PriNormal,
                        "DTD%d : Monitor Range Limits:\n", i);
                    Printf(Tee::PriNormal,
                        "           Min.Vertical Rate: %d Hz\n",
                        dtdb->Data[0]);
                    Printf(Tee::PriNormal,
                        "           Max.Vertical Rate: %d Hz\n",
                        dtdb->Data[1]);
                    Printf(Tee::PriNormal,
                        "         Min.Horizontal Rate: %d KHz\n",
                        dtdb->Data[2]);
                    Printf(Tee::PriNormal,
                        "         Max.Horizontal Rate: %d KHz\n",
                        dtdb->Data[3]);
                    Printf(Tee::PriNormal,
                        "        Max.Supported PixClk: %d MHz\n",
                        dtdb->Data[4]*10);
                    break;
                case DTT_MONITOR_NAME:
                    Printf(Tee::PriNormal, "DTD%d : Monitor Name: ", i);
                    break;
                case DTT_ADD_COLOR_POINT_DATA:
                    Printf(Tee::PriNormal, "DTD%d : Color Point:\n", i);
                    if (dtdb->Data[0])
                    {
                        Printf(Tee::PriNormal,
                            "        White Point index: %d\n", dtdb->Data[0]);
                        Printf(Tee::PriNormal,
                            "           White Low Bits: %d\n", dtdb->Data[1]);
                        Printf(Tee::PriNormal,
                            "                  White_x: %d\n", dtdb->Data[2]);
                        Printf(Tee::PriNormal,
                            "                  White_y: %d\n", dtdb->Data[3]);
                        Printf(Tee::PriNormal,
                            "              White Gamma: %d\n", dtdb->Data[4]);
                    }
                    if (dtdb->Data[5])
                    {
                        Printf(Tee::PriNormal,
                            "        White Point index: %d\n", dtdb->Data[5]);
                        Printf(Tee::PriNormal,
                            "           White Low Bits: %d\n", dtdb->Data[6]);
                        Printf(Tee::PriNormal,
                            "                  White_x: %d\n", dtdb->Data[7]);
                        Printf(Tee::PriNormal,
                            "                  White_y: %d\n", dtdb->Data[8]);
                        Printf(Tee::PriNormal,
                            "              White Gamma: %d\n", dtdb->Data[9]);
                    }
                    break;
                case DTT_ADD_STANDARD_TIMINGS:
                    Printf(Tee::PriNormal,
                        "DTD%d : Additional Standard Timings:\n", i);
                    for (j = 0, left = 0; j < 6; j++)
                    {
                        if ( (dtdb->Data[j*2 + 0] != 0x01) &&
                             (dtdb->Data[j*2 + 1] != 0x01) )
                        {
                            h = ((dtdb->Data[j*2 + 0]+31)*8);

                            v = (h/((UINT16)
                                hratios[((dtdb->Data[j*2 + 1] & 0xC0) >> 6)]));
                            v *= (UINT16)
                                vratios[((dtdb->Data[j*2 + 1] & 0xC0) >> 6)];

                            Printf(Tee::PriNormal,
                                 "  Mode %d [%04X]: %-4d x %4d @ %dHz", i,
                                 ( (dtdb->Data[j*2 + 1]<<8) |
                                   (dtdb->Data[j*2 + 0]   ) ),
                                 h, v,
                                 ((dtdb->Data[j*2 + 1] & 0x3F)+60) );

                            // Toggle left/right indicator
                            left ^= 0x01;
                            if (left)
                            {
                                Printf(Tee::PriNormal, "   ");
                            }
                            else
                            {
                                Printf(Tee::PriNormal, "\n");
                            }
                        }
                    }
                    if (!(left ^= 0x01))
                    {
                        Printf(Tee::PriNormal, "\n");
                    }
                    break;
                default:
                    Printf(Tee::PriNormal,
                        "DTD%d : Unknown Data Type Tag: 0x%x:\n",
                        i, dtdb->DataTypeTag);
                    Printf(Tee::PriNormal, "       Data (hex):");
                    for(j=0; j<13; j++)
                    {
                        Printf(Tee::PriNormal, " %02x", dtdb->Data[j]);
                        if ((j%4)==3)
                        {
                            Printf(Tee::PriNormal, " ");
                        }
                    }
                    Printf(Tee::PriNormal, "\n");
                    break;
            }

            // Print out ascit strings here..
            switch(dtdb->DataTypeTag)
            {
                case DTT_MONITOR_SERIAL_NUMBER:
                case DTT_ASCII_STRING:
                case DTT_MONITOR_NAME:
                    for(j=0; j<13; j++)
                    {
                        if (dtdb->Data[j] == 0xA)
                        {
                            break;
                        }
                        asciistr[j] = dtdb->Data[j];
                    }
                    asciistr[j] = 0;
                    Printf(Tee::PriNormal, "%s\n", asciistr);
                    break;
                default:
                    break;
            }

        }
    }
}

//! \brief Describes CEA 861 EDID Extension.
//!
//! This function takes a pointer to a byte buffer and then describes the
//! CEA 861 EDID Extension.
void DetectEdidTest::PrintCEABlock1(UINT08  *blk)
{
    UINT08  d = blk[2];
    UINT32  i, total;

    if (d<4)
    {
        Printf(Tee::PriNormal,
            "CEAB1: No Detailed Timing exists: d = %d\n", d);
    }
    else
    {
        Printf(Tee::PriNormal,
            "CEA EDID Timing Extension Block: Version 1\n");

        total = (127-d)/18;

        for(i=0; i<total; i++)
        {
            PrintDTB(i, &blk[d+18*i]);
        }
    }
}

//! \brief Describes CEA 861A EDID Extension.
//!
//! This function takes a pointer to a byte buffer and then describes the
//! CEA 861A EDID Extension.
void DetectEdidTest::PrintCEABlock2(UINT08  *blk)
{
    UINT08  d = blk[2];
    UINT32  i, total;

    if (d<4)
    {
        Printf(Tee::PriNormal,
            "CEAB2: No Detailed Timing exists: d = %d\n", d);
    }
    else
    {
        Printf(Tee::PriNormal,
            "CEA EDID Timing Extension Block: Version 2\n");

        Printf(Tee::PriNormal,
            "  Total native (preferred) formats: %d\n", (blk[3]&0xF));
        if (blk[3]&0x80)
        {
            Printf(Tee::PriNormal, "  Monitor supports underscan\n");
        }
        if (blk[3]&0x40)
        {
            Printf(Tee::PriNormal, "  Monitor supports basic audio\n");
        }
        if (blk[3]&0x20)
        {
            Printf(Tee::PriNormal, "  Monitor supports YCbCr 4:4:4\n");
        }
        if (blk[3]&0x10)
        {
            Printf(Tee::PriNormal, "  Monitor supports YCbCr 4:2:2\n");
        }

        total = (127-d)/18;

        for(i=0; i<total; i++)
        {
            PrintDTB(i, &blk[d+18*i]);
        }
    }
}

//! \brief Describes CEA 861B EDID Extension.
//!
//! This function takes a pointer to a byte buffer and then describes the
//! CEA 861B EDID Extension.
void DetectEdidTest::PrintCEABlock3(UINT08  *blk)
{
    UINT08 d = blk[2];
    UINT32 i, total, j, k, m;
    UINT08 tagcode  = 0;
    UINT08 dblength = 0;
    UINT08 audioformatcode;
    UINT08 maxchannels;

    if (d<4)
    {
        Printf(Tee::PriNormal,
            "CEAB3: No Detailed Timing exists: d = %d\n", d);
    }
    else
    {
        Printf(Tee::PriNormal,
            "CEA EDID Timing Extension Block: Version 3\n");

        Printf(Tee::PriNormal,
            "  Total native (preferred) formats: %d\n", (blk[3]&0xF));
        if (blk[3]&0x80)
        {
            Printf(Tee::PriNormal, "  Monitor supports underscan\n");
        }
        if (blk[3]&0x40)
        {
            Printf(Tee::PriNormal, "  Monitor supports basic audio\n");
        }
        if (blk[3]&0x20)
        {
            Printf(Tee::PriNormal, "  Monitor supports YCbCr 4:4:4\n");
        }
        if (blk[3]&0x10)
        {
            Printf(Tee::PriNormal, "  Monitor supports YCbCr 4:2:2\n");
        }

        i = 4;
        j = 0;
        while(i<d)
        {
            Printf(Tee::PriNormal, "Data Block %d: [%x] ", j, blk[i]);
            tagcode  = (blk[i]>>5);
            dblength = (blk[i]&0x1F);
            i++;
            switch(tagcode)
            {
                case 1:
                    Printf(Tee::PriNormal,
                        "Audio Data Block:  Length: %d\n", dblength);
                    for(k=0; k<dblength; k+=3)
                    {
                        Printf(Tee::PriNormal,
                            "  CEA Short Audio Descriptor %d:  "
                            "[%02x %02x %02x]\n",
                            dblength/3, blk[i+k], blk[i+k+1], blk[i+k+2]);
                        audioformatcode = ((blk[i+k]>>3)&0xF);
                        maxchannels = (blk[i+1]&0x7);
                        switch(audioformatcode)
                        {
                            case 1:
                                Printf(Tee::PriNormal,
                                    "  Linear PCM:   Max Channels: %d\n",
                                    maxchannels);
                                break;
                            case 2:
                                Printf(Tee::PriNormal,
                                    "        AC-3:   Max Channels: %d\n",
                                    maxchannels);
                                break;
                            case 3:
                                Printf(Tee::PriNormal,
                                    "       MPEG1:   Max Channels: %d\n",
                                    maxchannels);
                                break;
                            case 4:
                                Printf(Tee::PriNormal,
                                    "         MP3:   Max Channels: %d\n",
                                    maxchannels);
                                break;
                            case 5:
                                Printf(Tee::PriNormal,
                                    "       MPEG2:   Max Channels: %d\n",
                                    maxchannels);
                                break;
                            case 6:
                                Printf(Tee::PriNormal,
                                    "         AAC:   Max Channels: %d\n",
                                    maxchannels);
                                break;
                            case 7:
                                Printf(Tee::PriNormal,
                                    "         DTS:   Max Channels: %d\n",
                                    maxchannels);
                                break;
                            case 8:
                                Printf(Tee::PriNormal,
                                    "       ATARC:   Max Channels: %d\n",
                                    maxchannels);
                                break;
                            default:
                                Printf(Tee::PriNormal,
                                    "  Unsupported Audio Format Code: %d\n",
                                    ((blk[i+1]>>3)&0xF));
                                break;
                        }
                        //
                        // Now print out Rates information on the specifics
                        // of Audio Format Codes
                        //
                        switch(audioformatcode)
                        {
                            case 1:  // Case 1-8 are all the same
                            case 2:
                            case 3:
                            case 4:
                            case 5:
                            case 6:
                            case 7:
                            case 8:
                                if (blk[i+k+1]&0x7F)
                                {
                                    Printf(Tee::PriNormal, "    Rates:");
                                }
                                if (blk[i+k+1]&0x01)
                                {
                                    Printf(Tee::PriNormal, " 32KHz");
                                }
                                if (blk[i+k+1]&0x02)
                                {
                                    Printf(Tee::PriNormal, " 44KHz");
                                }
                                if (blk[i+k+1]&0x04)
                                {
                                    Printf(Tee::PriNormal, " 48KHz");
                                }
                                if (blk[i+k+1]&0x08)
                                {
                                    Printf(Tee::PriNormal, " 88KHz");
                                }
                                if (blk[i+k+1]&0x10)
                                {
                                    Printf(Tee::PriNormal, " 96KHz");
                                }
                                if (blk[i+k+1]&0x20)
                                {
                                    Printf(Tee::PriNormal, " 176KHz");
                                }
                                if (blk[i+k+1]&0x40)
                                {
                                    Printf(Tee::PriNormal, " 192KHz");
                                }
                                if (blk[i+k+1]&0x7F)
                                {
                                    Printf(Tee::PriNormal, "\n");
                                }
                                break;
                            default:
                                break;
                        }
                        // Now print out the 3rd byte information
                        switch(audioformatcode)
                        {
                            case 1:
                                if (blk[i+k+2]&0x07)
                                {
                                    Printf(Tee::PriNormal,
                                        "    Sample Sizes:");
                                }
                                if (blk[i+k+2]&0x01)
                                {
                                    Printf(Tee::PriNormal, "  16 bit");
                                }
                                if (blk[i+k+2]&0x02)
                                {
                                    Printf(Tee::PriNormal, "  20 bit");
                                }
                                if (blk[i+k+2]&0x04)
                                {
                                    Printf(Tee::PriNormal, "  24 bit");
                                }
                                if (blk[i+k+2]&0x07)
                                {
                                    Printf(Tee::PriNormal, "\n");
                                }
                                break;
                            case 2:
                            case 3:
                            case 4:
                            case 5:
                            case 6:
                            case 7:
                            case 8:
                                Printf(Tee::PriNormal,
                                    "    Maximum Bit Rate: %dKHz\n",
                                    (blk[i+k+2]*8));
                                break;
                            default:
                                break;
                        }
                    } // loop on k (Short Audio Descriptor Entry)

                    break;
                case 2:
                    Printf(Tee::PriNormal,
                        "Video Data Block:  Length: %d\n", dblength);
                    if (dblength)
                    {
                        Printf(Tee::PriNormal,
                            "  idx Code       HPix  VPix  i/p"
                            "  VFreq       ARatio\n");
                        for(m=0; m<dblength; m++)
                        {
                            switch(blk[i+m]&0x7F)
                            {
                                case  1:
                                    Printf(Tee::PriNormal, "  %2d     1"
                                        "        640   480   p   59.94/60Hz"
                                        "    4:3\n", m);
                                case  2:
                                    Printf(Tee::PriNormal, "  %2d     2"
                                        "        720   480   p   59.94/60Hz"
                                        "    4:3\n", m);
                                case  3:
                                    Printf(Tee::PriNormal, "  %2d     3"
                                        "        720   480   p   59.94/60Hz"
                                        "   16:9\n", m);
                                case  4:
                                    Printf(Tee::PriNormal, "  %2d     4"
                                        "       1280   720   p   59.94/60Hz"
                                        "   16:9\n", m);
                                case  5:
                                    Printf(Tee::PriNormal, "  %2d     5"
                                        "       1920  1080   i   59.94/60Hz"
                                        "   16:9\n", m);
                                case  6:
                                    Printf(Tee::PriNormal, "  %2d     6"
                                        "  720(1440)   480   i   59.94/60Hz"
                                        "    4:3\n", m);
                                case  7:
                                    Printf(Tee::PriNormal, "  %2d     7"
                                        "  720(1440)   480   i   59.94/60Hz"
                                        "   16:9\n", m);
                                case  8:
                                    Printf(Tee::PriNormal, "  %2d     8"
                                        "  720(1440)   240   p   59.94/60Hz"
                                        "    4:3\n", m);
                                case  9:
                                    Printf(Tee::PriNormal, "  %2d     9"
                                        "  720(1440)   240   p   59.94/60Hz"
                                        "   16:9\n", m);
                                case 10:
                                    Printf(Tee::PriNormal, "  %2d    10"
                                        "     (2880)   480   i   59.94/60Hz"
                                        "    4:3\n", m);
                                case 11:
                                    Printf(Tee::PriNormal, "  %2d    11"
                                        "     (2880)   480   i   59.94/60Hz"
                                        "   16:9\n", m);
                                case 12:
                                    Printf(Tee::PriNormal, "  %2d    12"
                                        "     (2880)   240   p   59.94/60Hz"
                                        "    4:3\n", m);
                                case 13:
                                    Printf(Tee::PriNormal, "  %2d    13"
                                        "     (2880)   240   p   59.94/60Hz"
                                        "   16:9\n", m);
                                case 14:
                                    Printf(Tee::PriNormal, "  %2d    14"
                                        "       1440   480   p   59.94/60Hz"
                                        "    4:3\n", m);
                                case 15:
                                    Printf(Tee::PriNormal, "  %2d    15"
                                        "       1440   480   p   59.94/60Hz"
                                        "   16:9\n", m);
                                case 16:
                                    Printf(Tee::PriNormal, "  %2d    16"
                                        "       1920  1080   p   59.94/60Hz"
                                        "   16:9\n", m);
                                case 17:
                                    Printf(Tee::PriNormal, "  %2d    17"
                                        "        720   576   p      50 Hz  "
                                        "    4:3\n", m);
                                case 18:
                                    Printf(Tee::PriNormal, "  %2d    18"
                                        "        720   576   p      50 Hz  "
                                        "   16:9\n", m);
                                case 19:
                                    Printf(Tee::PriNormal, "  %2d    19"
                                        "       1280   720   p      50 Hz  "
                                        "   16:9\n", m);
                                case 20:
                                    Printf(Tee::PriNormal, "  %2d    20"
                                        "       1920  1080   i      50 Hz  "
                                        "   16:9\n", m);
                                case 21:
                                    Printf(Tee::PriNormal, "  %2d    21"
                                        "  720(1440)   576   i      50 Hz  "
                                        "    4:3\n", m);
                                case 22:
                                    Printf(Tee::PriNormal, "  %2d    22"
                                        "  720(1440)   576   i      50 Hz  "
                                        "   16:9\n", m);
                                case 23:
                                    Printf(Tee::PriNormal, "  %2d    23"
                                        "  720(1440)   288   p      50 Hz  "
                                        "    4:3\n", m);
                                case 24:
                                    Printf(Tee::PriNormal, "  %2d    24"
                                        "  720(1440)   288   p      50 Hz  "
                                        "   16:9\n", m);
                                case 25:
                                    Printf(Tee::PriNormal, "  %2d    25"
                                        "     (2880)   576   i      50 Hz  "
                                        "    4:3\n", m);
                                case 26:
                                    Printf(Tee::PriNormal, "  %2d    26"
                                        "     (2880)   576   i      50 Hz  "
                                        "   16:9\n", m);
                                case 27:
                                    Printf(Tee::PriNormal, "  %2d    27"
                                        "     (2880)   288   p      50 Hz  "
                                        "    4:3\n", m);
                                case 28:
                                    Printf(Tee::PriNormal, "  %2d    28"
                                        "     (2880)   288   p      50 Hz  "
                                        "   16:9\n", m);
                                case 29:
                                    Printf(Tee::PriNormal, "  %2d    29"
                                        "       1440   576   p      50 Hz  "
                                        "    4:3\n", m);
                                case 30:
                                    Printf(Tee::PriNormal, "  %2d    30"
                                        "       1440   576   p      50 Hz  "
                                        "   16:9\n", m);
                                case 31:
                                    Printf(Tee::PriNormal, "  %2d    31"
                                        "       1920  1080   p      50 Hz  "
                                        "   16:9\n", m);
                                case 32:
                                    Printf(Tee::PriNormal, "  %2d    32"
                                        "       1920  1080   p   23.97/24Hz"
                                        "   16:9\n", m);
                                case 33:
                                    Printf(Tee::PriNormal, "  %2d    33"
                                        "       1920  1080   p      25 Hz  "
                                        "   16:9\n", m);
                                case 34:
                                    Printf(Tee::PriNormal, "  %2d    34"
                                        "       1920  1080   p   29.97/30Hz"
                                        "   16:9\n", m);
                                default:
                                    Printf(Tee::PriNormal, "  %2d  %2d "
                                        "  Unsupported output format\n",
                                           m, blk[i+m]&0x7F);
                            }
                        }
                    }
                    break;
                case 3:
                    Printf(Tee::PriNormal,
                        "Vendor Specific Data Block:  Length: %d\n", dblength);
                    Printf(Tee::PriNormal,
                           "    IEEE Registration Intentifier: 0x%06x  "
                           "[%02x %02x %02x]\n",
                           (  ((UINT32)(blk[i]))      |
                             (((UINT32)blk[i+1])<<8)  |
                             (((UINT32)blk[i+2])<<16) ),
                           blk[i], blk[i+1], blk[i+2]);
                    if (dblength > 3)
                    {
                        Printf(Tee::PriNormal,
                            "    Vendor Specific Data Block Payload:\n");

                        for (m = 3; m <= ((UINT32)dblength)-8; m += 8)
                        {
                            Printf(Tee::PriNormal,
                                   "        [%02x %02x %02x %02x"
                                   "  %02x %02x %02x %02x]\n",
                                   blk[i+m],   blk[i+m+1],
                                   blk[i+m+2], blk[i+m+3],
                                   blk[i+m+4], blk[i+m+5],
                                   blk[i+m+6], blk[i+m+7]);
                        }
                        if (m < ((UINT32)dblength))
                        {
                            Printf(Tee::PriNormal, "        [");
                            for( ; m < ((UINT32)dblength); m++)
                            {
                                Printf(Tee::PriNormal, "%02x ", blk[i+m]);
                                if ((m%8)==6)
                                {
                                    Printf(Tee::PriNormal, " ");
                                }
                            }
                            for (m = 8-((((UINT32)dblength)-3)%8); m > 0; m--)
                            {
                                Printf(Tee::PriNormal, "  ");
                                if ((m-1) > 0)
                                {
                                    if ((m%8)==5)
                                      Printf(Tee::PriNormal, " ");
                                }
                                else
                                {
                                    Printf(Tee::PriNormal, "]\n");
                                }
                            }
                        }
                    }
                    break;
                case 4:
                    Printf(Tee::PriNormal,
                          "Speaker Allocation Data Block: "
                          " [%02x %02x %02x]\n",
                           blk[i], blk[i+1], blk[i+2]);
                    Printf(Tee::PriNormal, "  Speakers Present:");
                    if (blk[i]&0x01)
                    {
                        Printf(Tee::PriNormal, " FL/FR");
                    }
                    if (blk[i]&0x02)
                    {
                        Printf(Tee::PriNormal, " LFE");
                    }
                    if (blk[i]&0x04)
                    {
                        Printf(Tee::PriNormal, " FC");
                    }
                    if (blk[i]&0x08)
                    {
                        Printf(Tee::PriNormal, " RL/RR");
                    }
                    if (blk[i]&0x10)
                    {
                        Printf(Tee::PriNormal, " RC");
                    }
                    if (blk[i]&0x20)
                    {
                        Printf(Tee::PriNormal, " FLC/FRC");
                    }
                    if (blk[i]&0x40)
                    {
                        Printf(Tee::PriNormal, " RLC/RRC");
                    }
                    break;
                default:
                    Printf(Tee::PriNormal,
                        "Unsupported Data Block Tag Code: %d\n", tagcode);
                    break;
            } // switch on data block tag code

            // OK.. increment i to the next block, and j to the next index
            i += dblength;
            j++;
        }

        total = (127-d)/18;

        for (i=0; i<total; i++)
        {
            PrintDTB(i, &blk[d+18*i]);
        }
    }
}

//! \brief Routes CEA EDID Extension for description
//!
//! This function takes a pointer to a byte buffer, decodes the current CEA
//! version, and routes the call to the correct decoding function.
void DetectEdidTest::PrintCEABlock(UINT08 *blk)
{
    switch(blk[1])
    {
        case 1:
            PrintCEABlock1(blk);
            break;
        case 2:
            PrintCEABlock2(blk);
            break;
        case 3:
            PrintCEABlock3(blk);
            break;
        default:
            Printf(Tee::PriNormal,
                "Unsupported CEA EDID Timing Extension Version: %d\n",
                blk[1]);
            break;
    }
}

//! \brief Describes DI EDID Extension block
//!
//! This function takes a pointer to a byte buffer and describes the
//! DI Extension block
void DetectEdidTest::PrintDI_EXTBlock(UINT08 *blk)
{
    DI_EXT *diExt = (DI_EXT *)blk;
    UINT32   i;

    Printf(Tee::PriNormal, "DI-EXT: Block Header:%02xh   Version Number: %d\n",
            diExt->header, diExt->version);

    // 3.2 Digital Interface
    if (diExt->digital.standard == 0)
    {
        Printf(Tee::PriNormal, "  Digital Interface: Analog Video Input\n");

        // check to make sure that bytes 3->b are all 0's
        if (blk[0x3] | blk[0x4] | blk[0x5] | blk[0x6] |
            blk[0x7] | blk[0x8] | blk[0x9] | blk[0xa] | blk[0xb])
        {
            Printf(Tee::PriNormal,
                "    Error! Analog Video Input should have bytes "
                "0x2->0xb as all 0's!\n");
        }
    }
    else
    {
        UINT16 DigitalClockMaxlinkFreq =
              (((UINT16)diExt->digital.clock.maxlinkfreqhi)<<8) |
               ((UINT16)diExt->digital.clock.maxlinkfreqlo);
        UINT16 DigitalClockCrossoverFreq =
              (((UINT16)diExt->digital.clock.crossoverhi)<<8) |
               ((UINT16)diExt->digital.clock.crossoverlo);

        Printf(Tee::PriNormal, "  Digital Interface: ");
        switch(diExt->digital.standard)
        {
            case 0x01:
                Printf(Tee::PriNormal, " Standard not defined\n");
                break;
            case 0x02:
                Printf(Tee::PriNormal, " DVI - Single Link\n");
                break;
            case 0x03:
                Printf(Tee::PriNormal, " DVI - Dual Link, High Resolution\n");
                break;
            case 0x04:
                Printf(Tee::PriNormal, " DVI - Dual Link, High Color\n");
                break;
            case 0x05:
                Printf(Tee::PriNormal, " DVI - Consumer Electronics\n");
                break;
            case 0x06:
                Printf(Tee::PriNormal, " Plug & Display\n");
                break;
            case 0x07:
                Printf(Tee::PriNormal, " Digital Flat Panel\n");
                break;
            case 0x08:
                Printf(Tee::PriNormal, " Open LDI - Single Link\n");
                break;
            case 0x09:
                Printf(Tee::PriNormal, " Open LDI - Dual Link\n");
                break;
            case 0x0a:
                Printf(Tee::PriNormal, " Open LDI - Consumer Electronics\n");
                break;
            default:
                Printf(Tee::PriNormal, " Undefined - ERROR!\n");
                break;
        }
        Printf(Tee::PriNormal, "    Version:");
        switch(diExt->digital.version.type & 0xC0)
        {
            default:
            case 0x00:
                Printf(Tee::PriNormal, " Not Specified\n");
                break;
            case 0x40:
                Printf(Tee::PriNormal,
                        " Release: %02d.%02d   Revision: %02d.%02d\n",
                        diExt->digital.version.type&0x3F,
                        diExt->digital.version.year,
                        diExt->digital.version.month,
                        diExt->digital.version.day);
                if (diExt->digital.version.year > 99)
                {
                    Printf(Tee::PriNormal,
                        "    ERROR: Release decimal portion (byte 4)"
                        " should be < 100!\n");
                }
                if (diExt->digital.version.day > 99)
                {
                    Printf(Tee::PriNormal,
                        "    ERROR: Revision decimal portion (byte 6)"
                        " should be < 100!\n");
                }
                break;
            case 0x80:
                Printf(Tee::PriNormal,
                    " Letter Designation: %c\n", diExt->digital.version.year);
                if ((diExt->digital.version.type&0x3F) |
                     diExt->digital.version.month |
                     diExt->digital.version.day)
                {
                    Printf(Tee::PriNormal,
                        "    ERROR: Letter designation requires byte 5"
                        " and 6 and Byte 4 bits [5:0] to be 0!\n");
                }
                break;
            case 0xc0:
                Printf(Tee::PriNormal, "  Date: %d - %d - %d\n",
                        1900+(UINT32)diExt->digital.version.year,
                        diExt->digital.version.month,
                        diExt->digital.version.day);
                if (diExt->digital.version.type&0x3F)
                {
                    Printf(Tee::PriNormal,
                        "    ERROR: Date code: requires Byte 4 bits [5:0]"
                        " to be 0!\n");
                }
                if ( (diExt->digital.version.month>12) ||
                     (diExt->digital.version.month == 0) )
                {
                    Printf(Tee::PriNormal,
                        "    ERROR: Date code: Month (Byte 5)"
                        " must be a number from 1 -> 12!\n");
                }
                if ( (diExt->digital.version.day>31) ||
                     (diExt->digital.version.day == 0) )
                {
                    Printf(Tee::PriNormal,
                           "    ERROR: Date code: Day (Byte 6)"
                           " must be a number from 1 -> 31!\n");
                }
                break;
        }
        Printf(Tee::PriNormal, "    Format:");
        if (diExt->digital.format.flags&0x80)
        {
            Printf(Tee::PriNormal, " Data Enable Polarity: ");
            if (diExt->digital.format.flags&0x40)
            {
                Printf(Tee::PriNormal, "high\n");
            }
            else
            {
                Printf(Tee::PriNormal, "low\n");
            }
        }
        else
        {
            Printf(Tee::PriNormal, " Data Enable not used\n");
            if (diExt->digital.format.flags&0x40)
            {
                Printf(Tee::PriNormal,
                    "      ERROR: Bit 6 of byte 7 should be 0 "
                    "when bit 7 is 0!\n");
            }
        }
        Printf(Tee::PriNormal, "            Edge of Shift Clock: ");
        switch(diExt->digital.format.flags&0x30)
        {
            default:
            case 0x00:
                Printf(Tee::PriNormal, "Not specified\n");
                break;
            case 0x10:
                Printf(Tee::PriNormal, "Rising Edge\n");
                break;
            case 0x20:
                Printf(Tee::PriNormal, "Falling Edge\n");
                break;
            case 0x30:
                Printf(Tee::PriNormal, "Both Rising and Falling edges\n");
                break;
        }
        Printf(Tee::PriNormal, "            HDCP: ");
        if (diExt->digital.format.flags&0x8)
        {
            Printf(Tee::PriNormal, "Supported\n");
        }
        else
        {
            Printf(Tee::PriNormal, "Not supported\n");
        }
        Printf(Tee::PriNormal, "            Double Input Clocking: ");
        if (diExt->digital.format.flags&0x4)
        {
            Printf(Tee::PriNormal, "Supported\n");
        }
        else
        {
            Printf(Tee::PriNormal, "Not supported\n");
        }
        Printf(Tee::PriNormal, "            Packetized Digital Video: ");
        if (diExt->digital.format.flags&0x2)
        {
            Printf(Tee::PriNormal, "Supported\n");
        }
        else
        {
            Printf(Tee::PriNormal, "Not supported\n");
        }
        Printf(Tee::PriNormal, "            Digital Data Format: ");
        switch(diExt->digital.format.data)
        {
            case 0x00:
                Printf(Tee::PriNormal, "Analog Video Input\n");
                break;
            case 0x15:
                Printf(Tee::PriNormal, "8-Bit Over 8-Bit RGB\n");
                break;
            case 0x19:
                Printf(Tee::PriNormal, "12-Bit Over 12-Bit RGB\n");
                break;
            case 0x24:
                Printf(Tee::PriNormal,
                    "24-Bit MSB-Aligned RGB {Single Link}\n");
                break;
            case 0x48:
                Printf(Tee::PriNormal,
                    "48-Bit MSB-Aligned RGB {Dual Link --- Hi-Resolution}\n");
                break;
            case 0x49:
                Printf(Tee::PriNormal,
                    "48-Bit MSB-Aligned RGB {Dual Link --- Hi-Color}\n");
                break;
            default:
                Printf(Tee::PriNormal, "Undefined  ERROR!\n");
                break;
        }
        Printf(Tee::PriNormal,
            "    Minimum Pixel Clock per Link: %d MHz\n",
            diExt->digital.clock.minlinkfreq);
        if ( (diExt->digital.clock.minlinkfreq == 0) ||
             (diExt->digital.clock.minlinkfreq == 0xFF) )
        {
            Printf(Tee::PriNormal,
                "      ERROR: Min Clock per Link must not be 0 or 255!\n");
        }
        Printf(Tee::PriNormal,
            "    Maximum Pixel Clock per Link: %d MHz\n",
            DigitalClockMaxlinkFreq);
        if ( (DigitalClockMaxlinkFreq == 0) ||
             (DigitalClockMaxlinkFreq == 0xFFFF) )
        {
            Printf(Tee::PriNormal,
                "      ERROR: Max Clock per Link must not be 0 or 65535!\n");
        }
        Printf(Tee::PriNormal, "    Crossover Frequency         : ");
        if (DigitalClockCrossoverFreq == 0)
        {
            Printf(Tee::PriNormal,
                "      ERROR: Crossover Frequency must not be 0!\n");
        }
        else if (DigitalClockCrossoverFreq == 0xFFFF)
        {
            Printf(Tee::PriNormal, "Single Link panel\n");
        }
        else
        {
            Printf(Tee::PriNormal, "%d MHz\n", DigitalClockCrossoverFreq);
        }
    }

    Printf(Tee::PriNormal, "  Sub-Pixel: Layout: ");
    switch(diExt->display.subpixel.layout)
    {
        case 0x00:
            Printf(Tee::PriNormal, "N/A    ");
            break;
        case 0x01:
            Printf(Tee::PriNormal, "RGB    ");
            break;
        case 0x02:
            Printf(Tee::PriNormal, "BGR    ");
            break;
        case 0x03:
            Printf(Tee::PriNormal, "Quad-G/");
            break;
        case 0x04:
            Printf(Tee::PriNormal, "Quad-G\\");
            break;
        default:
            Printf(Tee::PriNormal, "?ERROR!");
            break;
    }
    Printf(Tee::PriNormal, "  Config: ");
    switch(diExt->display.subpixel.config)
    {
        case 0x00:
            Printf(Tee::PriNormal, "N/A    ");
            break;
        case 0x01:
            Printf(Tee::PriNormal, "Delta  ");
            break;
        case 0x02:
            Printf(Tee::PriNormal, "Stripe ");
            break;
        case 0x03:
            Printf(Tee::PriNormal, "Str.Off");
            break;
        case 0x04:
            Printf(Tee::PriNormal, "Quad   ");
            break;
        default:
            Printf(Tee::PriNormal, "?ERROR!");
            break;
    }
    Printf(Tee::PriNormal, "  Shape: ");
    switch(diExt->display.subpixel.config)
    {
        case 0x00:
            Printf(Tee::PriNormal, "N/A\n");
            break;
        case 0x01:
            Printf(Tee::PriNormal, "round\n");
            break;
        case 0x02:
            Printf(Tee::PriNormal, "square\n");
            break;
        case 0x03:
            Printf(Tee::PriNormal, "rect\n");
            break;
        case 0x04:
            Printf(Tee::PriNormal, "oval\n");
            break;
        case 0x05:
            Printf(Tee::PriNormal, "ellipse\n");
            break;
        default:
            Printf(Tee::PriNormal, "?ERROR!\n");
            break;
    }

    Printf(Tee::PriNormal, "  Dot/Pixel Pitch: H: %d.%02dmm  V: %d.%02dmm\n",
            diExt->display.pixelpitch.horizontal/100,
            diExt->display.pixelpitch.horizontal%100,
            diExt->display.pixelpitch.vertical/100,
            diExt->display.pixelpitch.vertical%100);

    if (diExt->display.flags & 0x80)
    {
        Printf(Tee::PriNormal, "  Fixed Pixel Format: Present\n");
    }
    else
    {
        Printf(Tee::PriNormal, "  Fixed Pixel Format: Not present\n");
    }

    Printf(Tee::PriNormal, "  View Direction: ");
    switch(diExt->display.flags&0x60)
    {
        default:
        case 0x00:
            Printf(Tee::PriNormal, "Not Specified\n");
            break;
        case 0x20:
            Printf(Tee::PriNormal, "Direct View\n");
            break;
        case 0x40:
            Printf(Tee::PriNormal, "Reflected View\n");
            break;
        case 0x60:
            Printf(Tee::PriNormal, "Direct & Reflected View\n");
            break;
    }

    if (diExt->display.flags & 0x10)
    {
        Printf(Tee::PriNormal, "  Display Background: Transparent\n");
    }
    else
    {
        Printf(Tee::PriNormal, "  Display Background: Non-transparent\n");
    }

    Printf(Tee::PriNormal, "  Physical Implementation: ");
    switch(diExt->display.flags&0xc)
    {
        default:
        case 0x00:
            Printf(Tee::PriNormal, "Not Specified\n");
            break;
        case 0x04:
            Printf(Tee::PriNormal, "Large Image device for groups\n");
            break;
        case 0x08:
            Printf(Tee::PriNormal, "Desktop or personal display\n");
            break;
        case 0x0c:
            Printf(Tee::PriNormal, "Eyepiece display\n");
            break;
    }

    if (diExt->display.flags & 0x2)
    {
        Printf(Tee::PriNormal, "  DDC/CI: Supported\n");
    }
    else
    {
        Printf(Tee::PriNormal, "  DDC/CI: Not supported\n");
    }

    if (diExt->display.flags & 0x1)
    {
        Printf(Tee::PriNormal,
            "  ERROR: Major Display Device Characteristics (byte 13) "
            "bit 0 must be 0!\n");
    }

    if (diExt->caps.misc & 0x80)
    {
        Printf(Tee::PriNormal, "  Legacy VGA/DOS Timing Modes: Supported\n");
    }
    else
    {
        Printf(Tee::PriNormal,
            "  Legacy VGA/DOS Timing Modes: Not supported\n");
    }

    Printf(Tee::PriNormal, "  Stereo Video: ");
    switch(diExt->caps.misc&0x70)
    {
        case 0x00:
            Printf(Tee::PriNormal, "No Direct Stereo\n");
            break;
        case 0x10:
            Printf(Tee::PriNormal,
                "Field seq. stereo via stereo sync signal\n");
            break;
        case 0x20:
            Printf(Tee::PriNormal, "Auto-stereoscopic, column interleave\n");
            break;
        case 0x30:
            Printf(Tee::PriNormal, "auto-stereoscopic, line interleave\n");
            break;
        default:
            Printf(Tee::PriNormal, "ERROR! Do Not Use %d here!\n",
                ((diExt->caps.misc&0x70)>>4));
            break;
    }

    if (diExt->caps.misc & 0x8)
    {
        Printf(Tee::PriNormal, "  Scaler: Present\n");
    }
    else
    {
        Printf(Tee::PriNormal, "  Scaler: Not available\n");
    }

    if (diExt->caps.misc & 0x4)
    {
        Printf(Tee::PriNormal, "  Image Centering: Present\n");
    }
    else
    {
        Printf(Tee::PriNormal, "  Image Centering: Not available\n");
    }

    if (diExt->caps.misc & 0x2)
    {
        Printf(Tee::PriNormal, "  Conditional Update: Supported\n");
    }
    else
    {
        Printf(Tee::PriNormal, "  Conditional Update: Not supported\n");
    }

    if (diExt->caps.misc & 0x1)
    {
        Printf(Tee::PriNormal, "  Interlaced Video: Supported\n");
    }
    else
    {
        Printf(Tee::PriNormal, "  Interlaced Video: Not supported\n");
    }

    Printf(Tee::PriNormal, "  Frame Rate Colwersion: ");
    switch(diExt->caps.framerate.flags&0x60)
    {
        default:
        case 0x00:
            Printf(Tee::PriNormal, "Not supported\n");
            break;
        case 0x20:
            Printf(Tee::PriNormal,
                "Vertical is colwerted to a single frequency\n");
            break;
        case 0x40:
            Printf(Tee::PriNormal,
                "Horizontal is colwerted to a single frequency\n");
            break;
        case 0x60:
            Printf(Tee::PriNormal,
                "Both Vertical & Horizontal are colwerted to single frequencies\n");
            break;
    }

    {
        UINT16 vFreq =
            (((UINT16)diExt->caps.framerate.vfreqhi)<<8) |
             ((UINT16)diExt->caps.framerate.vfreqlo);
        UINT16 hFreq =
            (((UINT16)diExt->caps.framerate.hfreqhi)<<8) |
             ((UINT16)diExt->caps.framerate.hfreqlo);

        Printf(Tee::PriNormal,
                "    Vertical: %d.%d Hz    Horizontal: %d.%d kHz\n",
                vFreq/100, vFreq%100,
                hFreq/100, hFreq%100);
    }
    if (diExt->caps.framerate.flags & 0x80)
    {
        Printf(Tee::PriNormal, "  Frame Lock: Supported\n");
    }
    else
    {
        Printf(Tee::PriNormal, "  Frame Lock: Not Supported\n");
    }

    Printf(Tee::PriNormal, "  Display Orientation: ");
    switch(diExt->caps.orientation&0xc0)
    {
        default:
        case 0x00:
            Printf(Tee::PriNormal, "Not defined\n");
            break;
        case 0x40:
            Printf(Tee::PriNormal, "Fixed (no rotate)\n");
            break;
        case 0x80:
            Printf(Tee::PriNormal, "Default (all possible states unknown)\n");
            break;
        case 0xc0:
            Printf(Tee::PriNormal,
                "Current (all possible states are known)\n");
            break;
    }
    if (diExt->caps.orientation & 0x20)
    {
        Printf(Tee::PriNormal, "  Screen Orientation: Portrait\n");
    }
    else
    {
        Printf(Tee::PriNormal, "  Screen Orientation: Landscape\n");
    }

    Printf(Tee::PriNormal, "  Zero Pixel Location: ");
    switch(diExt->caps.orientation&0x18)
    {
        default:
        case 0x00:
            Printf(Tee::PriNormal, "Upper Left\n");
            break;
        case 0x08:
            Printf(Tee::PriNormal, "Upper Right\n");
            break;
        case 0x10:
            Printf(Tee::PriNormal, "Lower Left\n");
            break;
        case 0x18:
            Printf(Tee::PriNormal, "Lower Right\n");
            break;
    }
    Printf(Tee::PriNormal, "  Scan Direction: ");
    switch(diExt->caps.orientation&0x6)
    {
        default:
        case 0x00:
            Printf(Tee::PriNormal, "Not Defined\n");
            break;
        case 0x02:
            Printf(Tee::PriNormal, "Fast:Long Axis  Slow:Short Axis\n");
            break;
        case 0x04:
            Printf(Tee::PriNormal, "Fast:Short Axis  Slow:Long Axis\n");
            break;
        case 0x06:
            Printf(Tee::PriNormal, "Undefined\n");
            break;
    }
    if (diExt->caps.orientation & 0x1)
    {
        Printf(Tee::PriNormal, "  Display is a Standalone Projector\n");
    }
    else
    {
        Printf(Tee::PriNormal, "  Display is a not a Standalone Projector\n");
    }

    Printf(Tee::PriNormal, "  Color/Luminance Decoding: Default  : ");
    switch(diExt->caps.colordecode.startup)
    {
        case 0x00:
            Printf(Tee::PriNormal, "Not defined\n");
            break;
        case 0x01:
            Printf(Tee::PriNormal, "BGR (additive color)\n");
            break;
        case 0x02:
            Printf(Tee::PriNormal,
                "Y/C (S-Video) NTSC color (luminance/chrominance on "
                "separate channels) per ITU-R BT.470-6 (SMPTE 170M)\n");
            break;
        case 0x03:
            Printf(Tee::PriNormal,
                "Y/C (S-Video) PAL color (luminance/chrominance on "
                "separate channels) per ITU-R BT.470-6\n");
            break;
        case 0x04:
            Printf(Tee::PriNormal,
                "Y/C (S-Video) SECAM color (luminance/ chrominance on "
                "separate channels) per ITU-R BT.470-6\n");
            break;
        case 0x05:
            Printf(Tee::PriNormal,
                "YCrCb per SMPTE 293M, SMPTE 294M (4:4:4)\n");
            break;
        case 0x06:
            Printf(Tee::PriNormal,
                "YCrCb per SMPTE 293M, SMPTE 294M (4:2:2)\n");
            break;
        case 0x07:
            Printf(Tee::PriNormal,
                "YCrCb per SMPTE 293M, SMPTE 294M (4:2:0)\n");
            break;
        case 0x08:
            Printf(Tee::PriNormal, "YCrCb per SMPTE 260M (Legacy HDTV)\n");
            break;
        case 0x09:
            Printf(Tee::PriNormal, "YPbPr per SMPTE 240M (Legacy HDTV)\n");
            break;
        case 0x0A:
            Printf(Tee::PriNormal, "YCrCb per SMPTE 274M (Modern HDTV)\n");
            break;
        case 0x0B:
            Printf(Tee::PriNormal, "YPbPr per SMPTE 274M (Modern HDTV)\n");
            break;
        case 0x0C:
            Printf(Tee::PriNormal, "Y B-Y R-Y BetaCam (Sony)\n");
            break;
        case 0x0D:
            Printf(Tee::PriNormal, "Y B-Y R-Y M-2 (Matsushita)\n");
            break;
        case 0x0E:
            Printf(Tee::PriNormal, "Monochrome\n");
            break;
        default:
            Printf(Tee::PriNormal,
                "ERROR! Do Not Use %d here!\n",
                diExt->caps.colordecode.startup);
            break;
    }

    Printf(Tee::PriNormal, "                            Preferred: ");
    switch(diExt->caps.colordecode.preferred)
    {
        case 0x00:
            Printf(Tee::PriNormal, "Not defined\n");
            break;
        case 0x01:
            Printf(Tee::PriNormal, "BGR (additive color)\n");
            break;
        case 0x02:
            Printf(Tee::PriNormal, "Y/C (S-Video)\n");
            break;
        case 0x03:
            Printf(Tee::PriNormal,
                "Yxx (SMPTE 2xxM), Color Difference (Component Video)\n");
            break;
        case 0x04:
            Printf(Tee::PriNormal, "Monochrome\n");
            break;
        default:
            Printf(Tee::PriNormal,
                "ERROR! Do Not Use %d here!\n",
                diExt->caps.colordecode.preferred);
            break;
    }

    if (!(diExt->caps.colordecode.caps1 | diExt->caps.colordecode.caps2) )
    {
        Printf(Tee::PriNormal, "    Capabilities: None\n");
    }
    else
    {
        Printf(Tee::PriNormal, "    Capabilities:\n");
        if (diExt->caps.colordecode.caps1&0x80)
        {
            Printf(Tee::PriNormal, "      BGR (additive color)\n");
        }
        if (diExt->caps.colordecode.caps1&0x40)
        {
            Printf(Tee::PriNormal,
                "      Y/C (S-Video) NTSC color (luminance/ chrominance on"
                " separate channels) per ITU-R BT.470-6 (SMPTE 170M)\n");
        }
        if (diExt->caps.colordecode.caps1&0x20)
        {
            Printf(Tee::PriNormal,
                "      Y/C (S-Video) PAL color (luminance/chrominance on"
                " separate channels) per ITU-R BT.470-6\n");
        }
        if (diExt->caps.colordecode.caps1&0x10)
        {
            Printf(Tee::PriNormal,
                "      Y/C (S-Video) SECAM color (luminance/chrominance on"
                " separate channels) per ITU-R BT.470-6\n");
        }
        if (diExt->caps.colordecode.caps1&0x8)
        {
            Printf(Tee::PriNormal,
                "      YCrCb per SMPTE 293M, SMPTE 294M (4:4:4)\n");
        }
        if (diExt->caps.colordecode.caps1&0x4)
        {
            Printf(Tee::PriNormal,
                "      YCrCb per SMPTE 293M, SMPTE 294M (4:2:2)\n");
        }
        if (diExt->caps.colordecode.caps1&0x2)
        {
            Printf(Tee::PriNormal,
                "      YCrCb per SMPTE 293M, SMPTE 294M (4:2:0)\n");
        }
        if (diExt->caps.colordecode.caps1&0x1)
        {
            Printf(Tee::PriNormal,
                "      YCrCb per SMPTE 260M (Legacy HDTV)\n");
        }
        if (diExt->caps.colordecode.caps2&0x80)
        {
            Printf(Tee::PriNormal,
                "      YPbPr per SMPTE 240M (Legacy HDTV)\n");
        }
        if (diExt->caps.colordecode.caps2&0x40)
        {
            Printf(Tee::PriNormal,
                "      YCrCb per SMPTE 274M (Modern HDTV)\n");
        }
        if (diExt->caps.colordecode.caps2&0x20)
        {
            Printf(Tee::PriNormal,
                "      YPbPr per SMPTE 274M (Modern HDTV)\n");
        }
        if (diExt->caps.colordecode.caps2&0x10)
        {
            Printf(Tee::PriNormal, "      Y B-Y R-Y BetaCam (Sony)\n");
        }
        if (diExt->caps.colordecode.caps2&0x8)
        {
            Printf(Tee::PriNormal, "      Y B-Y R-Y M-2 (Matsushita)\n");
        }
        if (diExt->caps.colordecode.caps2&0x4)
        {
            Printf(Tee::PriNormal, "      Monochrome\n");
        }
    }
    if (diExt->caps.colordecode.caps2&0x3)
    {
        Printf(Tee::PriNormal,
            "      ERROR: Byte 1Eh: bits 1 & 0 must be set to 0\n");
    }

    if (diExt->caps.colordepth.flags & 0x80)
    {
        Printf(Tee::PriNormal,
            "  Monitor Color Depth:  Display uses dithering\n");
    }
    else
    {
        Printf(Tee::PriNormal,
            "  Monitor Color Depth:  Display does not use dithering\n");
    }
    Printf(Tee::PriNormal, "     Blue:");
    if (diExt->caps.colordepth.bgr.blue == 0x00)
    {
        Printf(Tee::PriNormal, "N/A   ");
    }
    else if (diExt->caps.colordepth.bgr.blue > 0x10)
    {
        Printf(Tee::PriNormal, "ERROR ");
    }
    else
    {
        Printf(Tee::PriNormal, "%2d bpc", diExt->caps.colordepth.bgr.blue);
    }
    Printf(Tee::PriNormal, "  Green:");
    if (diExt->caps.colordepth.bgr.green == 0x00)
    {
        Printf(Tee::PriNormal, "N/A   ");
    }
    else if (diExt->caps.colordepth.bgr.green > 0x10)
    {
        Printf(Tee::PriNormal, "ERROR ");
    }
    else
    {
        Printf(Tee::PriNormal, "%2d bpc", diExt->caps.colordepth.bgr.green);
    }
    Printf(Tee::PriNormal, "    Red:");
    if (diExt->caps.colordepth.bgr.red == 0x00)
    {
        Printf(Tee::PriNormal, "N/A\n");
    }
    else if (diExt->caps.colordepth.bgr.red > 0x10)
    {
        Printf(Tee::PriNormal, "ERROR\n");
    }
    else
    {
        Printf(Tee::PriNormal, "%2d bpc\n", diExt->caps.colordepth.bgr.red);
    }

    Printf(Tee::PriNormal, "    Cb/Pb:");
    if (diExt->caps.colordepth.ycrcb_yprpb.cb_pb == 0x00)
    {
        Printf(Tee::PriNormal, "N/A   ");
    }
    else if (diExt->caps.colordepth.ycrcb_yprpb.cb_pb > 0x10)
    {
        Printf(Tee::PriNormal, "ERROR ");
    }
    else
    {
        Printf(Tee::PriNormal, "%2d bpc",
            diExt->caps.colordepth.ycrcb_yprpb.cb_pb);
    }
    Printf(Tee::PriNormal, "      Y:");
    if (diExt->caps.colordepth.ycrcb_yprpb.y == 0x00)
    {
        Printf(Tee::PriNormal, "N/A   ");
    }
    else if (diExt->caps.colordepth.ycrcb_yprpb.y > 0x10)
    {
        Printf(Tee::PriNormal, "ERROR ");
    }
    else
    {
        Printf(Tee::PriNormal, "%2d bpc",
            diExt->caps.colordepth.ycrcb_yprpb.y);
    }
    Printf(Tee::PriNormal, "  Cr/Pr:");
    if (diExt->caps.colordepth.ycrcb_yprpb.cr_pr == 0x00)
    {
        Printf(Tee::PriNormal, "N/A\n");
    }
    else if (diExt->caps.colordepth.ycrcb_yprpb.cr_pr > 0x10)
    {
        Printf(Tee::PriNormal, "ERROR\n");
    }
    else
    {
        Printf(Tee::PriNormal, "%2d bpc\n",
            diExt->caps.colordepth.ycrcb_yprpb.cr_pr);
    }

    Printf(Tee::PriNormal, "  Aspect Modes: ");
    if (!(diExt->caps.aspect&0xf0) )
    {
        Printf(Tee::PriNormal, "None");
    }
    if (diExt->caps.aspect&0x80)
    {
        Printf(Tee::PriNormal, "Full");
    }
    if ( (diExt->caps.aspect&0x80) & (diExt->caps.aspect&0x40) )
    {
        Printf(Tee::PriNormal, ", ");
    }
    if (diExt->caps.aspect&0x40)
    {
        Printf(Tee::PriNormal, "Zoom");
    }
    if ( (diExt->caps.aspect&0xc0) & (diExt->caps.aspect&0x20) )
    {
        Printf(Tee::PriNormal, ", ");
    }
    if (diExt->caps.aspect&0x20)
    {
        Printf(Tee::PriNormal, "Squeeze");
    }
    if ( (diExt->caps.aspect&0xe0) & (diExt->caps.aspect&0x10) )
    {
        Printf(Tee::PriNormal, ", ");
    }
    if (diExt->caps.aspect&0x10)
    {
        Printf(Tee::PriNormal, "Variable");
    }
    Printf(Tee::PriNormal, "\n");
    if (diExt->caps.aspect&0xF)
    {
        Printf(Tee::PriNormal,
            "    ERROR: Aspect Modes (byte 26h) bits [0:3] Must be 0!");
    }

    for(i=0x27; i<0x51; i++)
    {
        if (blk[i])
        {
            Printf(Tee::PriNormal, "  ERROR: Byte %d must be set to 0!\n", i);
        }
    }

    if ( (diExt->gamma.lumcount & 0xc0) == 0x00 )
    {
        Printf(Tee::PriNormal,
            "  Display Transfer Characteristic: Not Defined\n");

        if (diExt->gamma.lumcount & 0x3f)
        {
            Printf(Tee::PriNormal,
                "  ERROR: Byte 51h bits [5:0] must be set to 0!\n");
        }

        for(i=0x52; i<0x7f; i++)
        {
            if (blk[i])
            {
                Printf(Tee::PriNormal,
                    "  ERROR: Byte %d must be set to 0!\n", i);
            }
        }
    }
    else if ( (diExt->gamma.lumcount & 0xc0) == 0x40 )
    {
        Printf(Tee::PriNormal,
                "  Display Transfer Characteristic: Defined as %d "
                "Luminance Values:\n",
               (diExt->gamma.lumcount & 0x3f));
        if ( (diExt->gamma.lumcount & 0x3f) > 45)
        {
            Printf(Tee::PriNormal,
                "    ERROR: Maximum of 45 Luminance Values!\n");
            diExt->gamma.lumcount = 0x40 | 45;
        }
        for (i=0; i<(UINT32)(diExt->gamma.lumcount&0x3F); i++)
        {
            if ((i%4) == 0)
            {
                Printf(Tee::PriNormal, "    %02x:", i);
            }
            Printf(Tee::PriNormal, " %02x", blk[i+0x52]);
            if ( ((i%4) == 3) ||
                 (i==(UINT32)((diExt->gamma.lumcount&0x3F)-1)) )
            {
                Printf(Tee::PriNormal, "\n");
            }
        }
    }
    else if ( (diExt->gamma.lumcount & 0xc0) == 0x80 )
    {
        Printf(Tee::PriNormal,
            "  Display Transfer Characteristic: Defined as %d RGB Values:\n",
            (diExt->gamma.lumcount & 0x3f));
        if ( (diExt->gamma.lumcount & 0x3f) > 15)
        {
            Printf(Tee::PriNormal,
                "    ERROR: Maximum of 15 RGB Values!\n");
            diExt->gamma.lumcount = 0x40 | 15;
        }
        Printf(Tee::PriNormal, "         B  G  R\n");
        for(i=0; i<(UINT32)(diExt->gamma.lumcount&0x3F); i++)
        {
            Printf(Tee::PriNormal, "    %02x: ", i);
            Printf(Tee::PriNormal, " %02x", diExt->gamma.blue[i]);
            Printf(Tee::PriNormal, " %02x", diExt->gamma.green[i]);
            Printf(Tee::PriNormal, " %02x", diExt->gamma.red[i]);
        }
    }
    else if ( (diExt->gamma.lumcount & 0xc0) == 0xc0 )
    {
        Printf(Tee::PriNormal,
            "  Display Transfer Characteristic: Reserved! ERROR!\n");
    }
}

// Print EDID information
//
void DetectEdidTest::PrintEdid1(UINT08 *ptr)
{
    EDID1  *edid = (EDID1 *)ptr;
    UINT16  i;
    // Left or right column?
    UINT08  left;
    UINT16  h, v;
    UINT32  hratios[] = { 16, 4, 5, 16 };
    UINT32  vratios[] = { 10, 3, 4,  9 };
    UINT08  smallversion = ((edid->edidversion<<4)&0xF0) |
                            (edid->edidrevision & 0xF);
    UINT32  chromax, chromay;
    UINT16  mfgEISAid = (((UINT16) edid->mfgEISAidhi)<<8) |
                         ((UINT16) edid->mfgEISAidlo);
    UINT16  ProductId = (((UINT16) edid->productcodehi)<<8) |
                         ((UINT16) edid->productcodelo);
    UINT32  serialNumber = (((UINT32) edid->serial[3])<<24) |
                           (((UINT32) edid->serial[2])<<16) |
                           (((UINT32) edid->serial[1])<<8) |
                           (((UINT32) edid->serial[0]));

    Printf(Tee::PriNormal, "\n");

    Printf(Tee::PriNormal,
        "EDID Version %d.%d\n", edid->edidversion, edid->edidrevision);

    Printf(Tee::PriNormal,
        "Mfg ID       [ %04X] : %c%c%c      "
        "Mfg Date [%02X.%02X] : Week %d, %d\n",
            mfgEISAid,
            (0x40+((mfgEISAid >> 2) & 0x001F)),
            (0x40+(((mfgEISAid >> 13) & 0x0007) |
             ((mfgEISAid & 0x0003) << 3))),
            (0x40+((mfgEISAid >> 8) & 0x001F)),
             edid->mfgdatemonth,
             edid->mfgdateyear,
             edid->mfgdatemonth,
             edid->mfgdateyear+1990);
    Printf(Tee::PriNormal,
        "Product Code [ %04X] : %06d   Serial Number [%08X] %09d\n",
             ProductId,
             ProductId,
             (UINT32)serialNumber,
             (UINT32)serialNumber);

    Printf(Tee::PriNormal,
        "\lwideo Input Definition: [%02X]: ", edid->videoinputtype);
    Printf(Tee::PriNormal, "  %s Input Signal",
        ((edid->videoinputtype & 0x80) ? "Digital" : "Analog"));

    // Digital
    if ((edid->videoinputtype & 0x80))
    {
        Printf(Tee::PriNormal, "%s",
            ((edid->videoinputtype & 0x01) ? " - VESA DFP 1.x TDMS" : ""));
    }
    Printf(Tee::PriNormal, "\n");

    Printf(Tee::PriNormal, "    Maximum Image Size: ");
    if (!(edid->imagesizeh) ||
        !(edid->imagesizev) )
    {
        Printf(Tee::PriNormal, "No assumptions on size\n");
    }
    else
    {
        Printf(Tee::PriNormal,
            " %d cm x %d cm\n", edid->imagesizeh, edid->imagesizev);
    }

    Printf(Tee::PriNormal, "      Display Gamma: %d.%d\n",
            ((edid->gamma+100)/100), ((edid->gamma+100)%100) );

    Printf(Tee::PriNormal,
        "      Display Features: [%02X]:\n      ", edid->features);
    if (edid->features & 0x80)
    {
        Printf(Tee::PriNormal, "  Standby");
    }
    if (edid->features & 0x40)
    {
        Printf(Tee::PriNormal, "  Suspend");
    }
    if (edid->features & 0x20)
    {
        Printf(Tee::PriNormal, "  Low/Off");
    }
    if (edid->features & 0xE0)
    {
        Printf(Tee::PriNormal, "\n");
    }
    if ((edid->features & 0x18) == 0x00)
    {
        Printf(Tee::PriNormal, "        Monochrome/Greyscale display\n");
    }
    else if ((edid->features & 0x18) == 0x08)
    {
        Printf(Tee::PriNormal, "        RGB Color Display\n");
    }
    else if ((edid->features & 0x18) == 0x10)
    {
        Printf(Tee::PriNormal, "        Non-RGB Multicolor Display\n");
    }
    else if ((edid->features & 0x18) == 0x18)
    {
        Printf(Tee::PriNormal, "        Undefined Display (colors)\n");
    }

    // This bit was reserved in EDID versions 1.0 and 1.1
    if (smallversion > 0x11)
    {
        if (edid->features & 0x04)
        {
            Printf(Tee::PriNormal,
                "       Display uses Standard Default Color Space\n");
        }
    }

    if (edid->features & 0x02)
    {
        Printf(Tee::PriNormal, "        Preferred Timing in DTB #0\n");
    }

    // This bit was reserved in EDID versions 1.0 and 1.1
    if (smallversion > 0x11)
    {
        if (edid->features & 0x01)
        {
            Printf(Tee::PriNormal, "        Display supports GTF Timing\n");
        }
    }

    // Determine these numbers for red first
    chromax = (edid->colorchars[2]<<2) | ((edid->colorchars[0] & 0xC0) >> 6);
    chromay = (edid->colorchars[3]<<2) | ((edid->colorchars[0] & 0x30) >> 4);
    // Now multiply to get the 3 digit value below the decimal.
    chromax = ((chromax*1000)>>10);
    chromay = ((chromay*1000)>>10);
    Printf(Tee::PriNormal,
        "Chromaticity:    (Red): (0.%3d, 0.%3d)\n", chromax, chromay);
    // Determine these numbers for green
    chromax = (edid->colorchars[4]<<2) | ((edid->colorchars[0] & 0xC) >> 2);
    chromay = (edid->colorchars[5]<<2) | ((edid->colorchars[0] & 0x3) >> 0);
    // Now multiply to get the 3 digit value below the decimal.
    chromax = ((chromax*1000)>>10);
    chromay = ((chromay*1000)>>10);
    Printf(Tee::PriNormal,
        "               (Green): (0.%3d, 0.%3d)\n", chromax, chromay);
    // Determine these numbers for blue
    chromax = (edid->colorchars[6]<<2) | ((edid->colorchars[1] & 0xC0) >> 6);
    chromay = (edid->colorchars[7]<<2) | ((edid->colorchars[1] & 0x30) >> 4);
    // Now multiply to get the 3 digit value below the decimal.
    chromax = ((chromax*1000)>>10);
    chromay = ((chromay*1000)>>10);
    Printf(Tee::PriNormal,
        "                (Blue): (0.%3d, 0.%3d)\n", chromax, chromay);
    // Determine these numbers for white
    chromax = (edid->colorchars[8]<<2) | ((edid->colorchars[1] & 0xC) >> 2);
    chromay = (edid->colorchars[9]<<2) | ((edid->colorchars[1] & 0x3) >> 0);
    // Now multiply to get the 3 digit value below the decimal.
    chromax = ((chromax*1000)>>10);
    chromay = ((chromay*1000)>>10);
    Printf(Tee::PriNormal,
        "               (White): (0.%3d, 0.%3d)\n", chromax, chromay);

    Printf(Tee::PriNormal, "\nEstablished Modes [%02X%02X%02X]:\n",
            edid->mfgtimings, edid->established[1],edid->established[0]);

    left = 0;
    if (edid->established[0] & 0x80)
    {
        Printf(Tee::PriNormal,
            "%-39s", "   720 x  400 @ 70Hz (IBM/Std VGA)");
        if (!(left ^= 0x01))
        {
            Printf(Tee::PriNormal, "\n");
        }
    }
    if (edid->established[0] & 0x40)
    {
        Printf(Tee::PriNormal,
            "%-39s", "   720 x  400 @ 88Hz (IBM/XGA)");
        if (!(left ^= 0x01))
        {
            Printf(Tee::PriNormal, "\n");
        }
    }

    if (edid->established[0] & 0x20)
    {
        Printf(Tee::PriNormal,
            "%-39s", "   640 x  480 @ 60Hz (IBM/Std VGA)");
        if (!(left ^= 0x01))
        {
            Printf(Tee::PriNormal, "\n");
        }
    }
    if (edid->established[0] & 0x10)
    {
        Printf(Tee::PriNormal,
            "%-39s", "   640 x  480 @ 67Hz (Apple/Mac)");
        if (!(left ^= 0x01))
        {
            Printf(Tee::PriNormal, "\n");
        }
    }
    if (edid->established[0] & 0x08)
    {
        Printf(Tee::PriNormal,
            "%-39s", "   640 x  480 @ 72Hz (VESA DMT)");
        if (!(left ^= 0x01))
        {
            Printf(Tee::PriNormal, "\n");
        }
    }
    if (edid->established[0] & 0x04)
    {
        Printf(Tee::PriNormal, "%-39s", "   640 x  480 @ 75Hz (VESA DMT)");
        if (!(left ^= 0x01))
        {
            Printf(Tee::PriNormal, "\n");
        }
    }

    if (edid->established[0] & 0x02)
    {
        Printf(Tee::PriNormal, "%-39s", "   800 x  600 @ 56Hz (VESA DMT)");
        if (!(left ^= 0x01))
        {
            Printf(Tee::PriNormal, "\n");
        }
    }
    if (edid->established[0] & 0x01)
    {
        Printf(Tee::PriNormal, "%-39s", "   800 x  600 @ 60Hz (VESA DMT)");
        if (!(left ^= 0x01))
        {
            Printf(Tee::PriNormal, "\n");
        }
    }
    if (edid->established[1] & 0x80)
    {
        Printf(Tee::PriNormal, "%-39s", "   800 x  600 @ 72Hz (VESA DMT)");
        if (!(left ^= 0x01))
        {
            Printf(Tee::PriNormal, "\n");
        }
    }
    if (edid->established[1] & 0x40)
    {
        Printf(Tee::PriNormal, "%-39s", "   800 x  600 @ 75Hz (VESA DMT)");
        if (!(left ^= 0x01))
        {
            Printf(Tee::PriNormal, "\n");
        }
    }

    if (edid->established[1] & 0x20)
    {
        Printf(Tee::PriNormal, "%-39s", "   832 x  624 @ 75Hz (Apple/Mac)");
        if (!(left ^= 0x01))
        {
            Printf(Tee::PriNormal, "\n");
        }
    }

    if (edid->established[1] & 0x10)
    {
        Printf(Tee::PriNormal, "%-39s",
            "  1024 x  768 @ 87Hz Interlaced (IBM)");
        if (!(left ^= 0x01))
        {
            Printf(Tee::PriNormal, "\n");
        }
    }
    if (edid->established[1] & 0x08)
    {
        Printf(Tee::PriNormal, "%-39s", "  1024 x  768 @ 60Hz (VESA DMT)");
        if (!(left ^= 0x01))
        {
            Printf(Tee::PriNormal, "\n");
        }
    }
    if (edid->established[1] & 0x04)
    {
        Printf(Tee::PriNormal, "%-39s", "  1024 x  768 @ 70Hz (VESA DMT)");
        if (!(left ^= 0x01))
        {
            Printf(Tee::PriNormal, "\n");
        }
    }
    if (edid->established[1] & 0x02)
    {
        Printf(Tee::PriNormal, "%-39s", "  1024 x  768 @ 75Hz (VESA DMT)");
        if (!(left ^= 0x01))
        {
            Printf(Tee::PriNormal, "\n");
        }
    }

    if (edid->mfgtimings & 0x80)
    {
        Printf(Tee::PriNormal, "%-39s", "  1152 x  870 @ 75Hz (Apple/Mac)");
        if (!(left ^= 0x01))
        {
            Printf(Tee::PriNormal, "\n");
        }
    }

    if (edid->established[0] & 0x01)
    {
        Printf(Tee::PriNormal, "%-39s", "  1280 x 1024 @ 75Hz (VESA DMT)");
        if (!(left ^= 0x01))
        {
            Printf(Tee::PriNormal, "\n");
        }

    }
    if (!(left ^= 0x01))
    {
        Printf(Tee::PriNormal, "\n");
    }

    Printf(Tee::PriNormal, "\nStandard Timing Modes:\n");
    for (i = 0, left = 0; i < 8; i+=2)
    {
        if ( (edid->stdtiming[i  ] != 0x01) &&
             (edid->stdtiming[i+1] != 0x01) )
        {
            h = (((edid->stdtiming[i] & 0xFF)+31)*8);

            v = (h/(UINT16)(hratios[((edid->stdtiming[i+1] & 0xC0) >> 6)]));
            v *= (UINT16)(vratios[((edid->stdtiming[i+1] & 0xC0) >> 6)]);

            Printf(Tee::PriNormal, "  Mode %d [%04X]: %-4d x %4d @ %dHz", (i>>1),
                 ((((UINT16)edid->stdtiming[i+1])<<8) |
                   ((UINT16)edid->stdtiming[i])), h, v,
                 ((edid->stdtiming[i+1] & 0x3F)+60) );

            left ^= 0x01;           // Toggle left/right indicator
            if (left)
            {
                Printf(Tee::PriNormal, "   ");
            }
            else
            {
                Printf(Tee::PriNormal, "\n");
            }
        }
    }
    if (!(left ^= 0x01))
    {
        Printf(Tee::PriNormal, "\n");
    }

    // Walk the Detailed Timing Descriptors
    Printf(Tee::PriNormal, "\nDetailed Timing Blocks:\n");
    for (i = 0, left = 0; i < 4; i++)
    {
        PrintDTB(i, (UINT08 *)&(edid->detailtiming[i]));
    } // loop on detailed timing blocks

    if (edid->Extensions == 1)
    {
        Printf(Tee::PriNormal, "\n");
        switch(ptr[0x80])
        {
            case 0x02:
                PrintCEABlock(&ptr[0x80]);
                break;
            case 0x40:
                PrintDI_EXTBlock(&ptr[0x80]);
                break;
            default:
                Printf(Tee::PriNormal,
                    "Unsupported Extension Block Type: %d\n", ptr[0x80]);
                break;
        }
    }
    else if (edid->Extensions)
    {
        Printf(Tee::PriNormal, "Unsupported: More than one EDID Extension.\n");
    }

    Printf(Tee::PriNormal, "\n");
    return;
}

void DetectEdidTest::PrintEdid2(UINT08 *ptr)
{
    Printf(Tee::PriNormal, "\n");

    Printf(Tee::PriNormal, "EDID2 block\n");

    Printf(Tee::PriNormal, "\n");
    return;
}

void DetectEdidTest::DescribeEdid(UINT08 *Edid)
{
    UINT08 edid1_header[] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};

    if (memcmp(Edid, edid1_header, 8) == 0)
    {
        PrintEdid1(Edid);       // EDID1 block
    }
    else if (Edid[0] == 0x20)
    {
        PrintEdid2(Edid);       // EDID2 block
    }

    return;
}

//! \brief EdidTests grabs edids for all possible displays and describes them
//!
//! This function takes a subdevice argument and runs edids on all displays
//! found on that one subdevice.  It will run a normal edid request. Then it
//! will ask for the cached edid state. It will compare the two to make sure
//! they match.  If they do, then it will describe the EDID.
//! If using RunMode == 2 (EDID) and the Display Mask argument is non-zero,
//! then we'll only describe the EDIDs that are in the DisplayMask argument.
RC DetectEdidTest::EdidTest(UINT32 subdev)
{
    LW0073_CTRL_SYSTEM_GET_SUPPORTED_PARAMS dispSupportedParams = {0};
    LW0073_CTRL_SPECIFIC_GET_EDID_PARAMS getEdidParams = {0};
    UINT32 i, j, status = 0;

    // get supported displays
    dispSupportedParams.subDeviceInstance = subdev;
    dispSupportedParams.displayMask = 0;
    status = LwRmControl(m_hClient, m_hDisplay,
                         LW0073_CTRL_CMD_SYSTEM_GET_SUPPORTED,
                         &dispSupportedParams, sizeof (dispSupportedParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriNormal,
            "DetectEdidTest: displays supported failed: 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }

    Printf(Tee::PriNormal,
           "  Supported displays: 0x%x\n",
           (UINT32)dispSupportedParams.displayMask);

    //
    // If running on EDID, check the displayMask
    //
    if ( (m_RunMode == LW_RUN_MODE_TYPE_EDID) &&
         (m_DisplayMask) )
    {
        if (!((m_DisplayMask &
                dispSupportedParams.displayMask) == m_DisplayMask) )
        {
            Printf(Tee::PriNormal,
                "DetectEdidTest: DisplayMask 0x%x must be subset of\n"
                "                supported displays, 0x%x!\n",
                m_DisplayMask,
                (UINT32)dispSupportedParams.displayMask);
            return RC::BAD_PARAMETER;
        }
    }

    for (i = 0; i < 32; i++)
    {
        if (!(((UINT32)BIT(i)) & (UINT32)dispSupportedParams.displayMask) )
        {
            continue;
        }
        if (((UINT32)BIT(i)) > (UINT32)dispSupportedParams.displayMask)
        {
            break;
        }

        //
        // If running on EDID, only run on the one's that
        // are in the user's displayMask
        //
        if ( (m_RunMode == LW_RUN_MODE_TYPE_EDID) &&
             (m_DisplayMask) )
        {
            if (!(((UINT32)BIT(i)) & m_DisplayMask) )
            {
                continue;
            }
        }

        memset(m_EdidBuffer, 0, 1024*sizeof(UINT08));

        getEdidParams.subDeviceInstance = subdev;
        getEdidParams.displayId = (UINT32)BIT(i);
        getEdidParams.pEdidBuffer = (LwP64) m_EdidBuffer;
        getEdidParams.bufferSize = 1024;
        getEdidParams.flags =
            LW0073_CTRL_SPECIFIC_GET_EDID_FLAGS_COPY_CACHE_NO;

        if (m_RunMode != LW_RUN_MODE_TYPE_DEFAULT)
        {
            getEdidParams.flags = m_CtrlFlags;
        }

        status = LwRmControl(m_hClient, m_hDisplay,
                             LW0073_CTRL_CMD_SPECIFIC_GET_EDID,
                             &getEdidParams, sizeof (getEdidParams));

        if (status != LW_OK)
        {
            Printf(Tee::PriNormal,
                   "  Display 0x%x has no Edid\n", BIT(i));
            continue;
        }

        if (m_RunMode == LW_RUN_MODE_TYPE_DEFAULT)
        {
            memset(m_CachedBuffer, 0, 1024*sizeof(UINT08));
            getEdidParams.pEdidBuffer = (LwP64) m_CachedBuffer;
            getEdidParams.bufferSize = 1024;
            getEdidParams.flags =
                LW0073_CTRL_SPECIFIC_GET_EDID_FLAGS_COPY_CACHE_YES;

            status = LwRmControl(m_hClient, m_hDisplay,
                                 LW0073_CTRL_CMD_SPECIFIC_GET_EDID,
                                 &getEdidParams, sizeof (getEdidParams));

            if (memcmp(m_EdidBuffer, m_CachedBuffer, 1024*sizeof(UINT08)))
            {
                Printf(Tee::PriNormal,
                    "  Display 0x%x changed EDID between "
                    "DDC and Cached version\n", BIT(i));
                continue;
            }
        }

        Printf(Tee::PriNormal,
               "  Display 0x%x Edid bytes:", BIT(i));

        if ((m_CtrlFlags&1)==1)
        {
            Printf(Tee::PriNormal, " Cached\n");
        }
        else
        {
            Printf(Tee::PriNormal, "\n");
        }

        for (j=0; j<1024; j++)
        {
          if ((j%16) == 0)
          {
              Printf(Tee::PriNormal, "\n    ");
          }
          Printf(Tee::PriNormal, "%02x ", m_EdidBuffer[j]);
          if ( (j%16) == 7)
          {
              Printf(Tee::PriNormal, " ");
          }

          if ( (j%0x80) == 0x7F)
          {
              if ( (j+1) == (UINT32)((m_EdidBuffer[0x7E]+1)*0x80) )
              {
                  break;
              }
          }
        }
        Printf(Tee::PriNormal, "\n\n");

        DescribeEdid(m_EdidBuffer);
    }

    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(DetectEdidTest, RmTest,
    "Detect and Edid Test:\n"
    "  This object contains 2 functionalities:\n"
    "   1. Detect and Edid Test: This test calls the RM to test \n"
    "      various ways of detection and it will atttempt to read \n"
    "      the EDID on all displayIDs.\n"
    "   2. Detect and Edid direct function calls: These calls allow\n"
    "      someone to directly call the Detection and Edid logic for\n"
    "      one gpu in the system and specify the displays or flags to"
    "      send to the RM.\n");

CLASS_PROP_READWRITE(DetectEdidTest, RunMode, UINT32,
    "Change the exelwtion of the Run() test: 0=Default, 1=Detect, 2=Edid.");
CLASS_PROP_READWRITE(DetectEdidTest, UserDevice, UINT32,
    "Device index to use when running Detect or Edid exclusively. "
    "Default is 0.");
CLASS_PROP_READWRITE(DetectEdidTest, UserSub, UINT32,
    "Subdevice index to use when running Detect or Edid exclusively. "
    "Default is 0.");
CLASS_PROP_READWRITE(DetectEdidTest, DisplayMask, UINT32,
    "DisplayMask to use when running Detect or Edid exclusively. "
    "Default (0) is all possible displays.");
CLASS_PROP_READWRITE(DetectEdidTest, CtrlFlags, UINT32,
    "The direct RmControl flags to use for Detect or Edid exclusively. "
    "0 means use the default flags.");

