/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2016,2019 by LWPU Corporation.  All rights reserved.  All
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
//! This test calls the RM to test the Display Port
//! the code lwrrently detects if any display port is connected to
//! the available GPUs.

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "lwRmApi.h"
//#include "rmt_dptest.h"
#include "lwmisc.h"

#include "class/cl0073.h"
#include "ctrl/ctrl0000.h"
#include "ctrl/ctrl0073.h"
#include "ctrl/ctrl0080.h"
#include "core/include/memcheck.h"

class DpTest : public RmTest
{
public:
    DpTest();
    virtual ~DpTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    //@{
    SETGET_PROP(GetAuxPort, UINT32);    //!< Function to grab JS variable GetAuxPort
    SETGET_PROP(RunMode, UINT32);    //!< Function to grab JS variable GetAuxPort
    SETGET_PROP(UserDevice, UINT32); //!< Function to grab JS variable UserDevice,
    SETGET_PROP(UserSub, UINT32);    //!< Function to grab JS variable UserSub
    SETGET_PROP(CtrlFlags, UINT32);  //!< Function to grab JS variable CtrlFlags
    //@}
private:
    // Run mode type
    #define LW_RUN_MODE_TYPE_DEFAULT                     (0x00000000)
    #define LW_RUN_MODE_TYPE_DETECT                      (0x00000001)
    #define LW_RUN_MODE_TYPE_EDID                        (0x00000002)
    //  0 = Normal run
    //  1 = Detect one gpu's displays
    //  2 = Grab an EDID from a GPU and if found, describe it.
    UINT32 m_RunMode;    //!< Current mode of operation during Run() command
    //private data
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
    //
    UINT32 m_CtrlFlags;
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
    UINT32 m_GetAuxPort;
private:
    RC DetectDpInDFP(UINT32 subdev);
    bool TestDpAuxChannel();
};

DpTest::DpTest()
{
    SetName("DpTest");
    m_RunMode            = 0;
    m_UserDevice         = 0;
    m_UserSub            = 0;
    m_DisplayMask        = 0;
    m_CtrlFlags          = 0;
    m_hClient            = 0;
    m_hDev               = 0;
    m_isDeviceAllocated  = false;
    m_isDisplayAllocated = false;
    m_GetAuxPort         = 0;
}

DpTest::~DpTest()
{

}

//! \brief IsTestSupported : test for support on current environment
//!
//! This test is mainly for real HW only.  It probably could be tested on
//! emulation, but for now, I'm just going to limit it to actual HW.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string DpTest::IsTestSupported()
{
    const UINT32 DisplayClasses[] =
        {
            LW04_DISPLAY_COMMON
        };
    UINT32 numDisplayClasses = sizeof( DisplayClasses ) / sizeof( DisplayClasses[0] );

    for ( UINT32 index = 0; index < numDisplayClasses; index++ )
    {
        if (IsClassSupported( DisplayClasses[index] ) == true )
            return RUN_RMTEST_TRUE;
    }

    return "No required display class is supported on this platform";
}

//! \brief Setup Function, used for the test related allocations
//!
//! Nothing to do here for Setup
//
//! \return Always passes
//------------------------------------------------------------------------------
RC DpTest::Setup()
{
    RC rc=OK;
    UINT32 status;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    m_hClient = 0;
    status = (UINT32)LwRmAllocRoot((LwU32*)&m_hClient);
    rc = RmApiStatusToModsRC(status);
    CHECK_RC(rc);
    return rc;
}

RC DpTest::DetectDpInDFP(UINT32 subdev)
{
    RC rc=OK;
    LW0073_CTRL_SYSTEM_GET_SUPPORTED_PARAMS dispSupportedParams = {0};
    UINT32 i, status = 0;

    // get supported displays
    dispSupportedParams.subDeviceInstance = subdev;
    dispSupportedParams.displayMask = 0;
    status = LwRmControl(m_hClient, m_hDisplay,
                         LW0073_CTRL_CMD_SYSTEM_GET_SUPPORTED,
                         &dispSupportedParams, sizeof (dispSupportedParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriNormal,
            "DpTest: displays supported failed: 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }

    Printf(Tee::PriNormal,
           "  Supported displays: 0x%x\n",
           (UINT32)dispSupportedParams.displayMask);

    //
    // Check the displayMask
    //
    if ( (m_RunMode == LW_RUN_MODE_TYPE_EDID) &&
         (m_DisplayMask) )
    {
        if (!((m_DisplayMask &
                dispSupportedParams.displayMask) == m_DisplayMask) )
        {
            Printf(Tee::PriNormal,
                "DpTest: DisplayMask 0x%x must be subset of\n"
                "                supported displays, 0x%x!\n",
                m_DisplayMask,
                (UINT32)dispSupportedParams.displayMask);
            return RC::BAD_PARAMETER;
        }
    }
    for (i = 16; i < 24; i++)//search for only DFP 0xFF0000
    {
        Printf(Tee::PriNormal,
                "DpTest: DisplayMask 0x%x \n",BIT(i));
        if (!(((UINT32)BIT(i)) & (UINT32)dispSupportedParams.displayMask) )
        {
            continue;
        }
        if (((UINT32)BIT(i)) > (UINT32)dispSupportedParams.displayMask)
        {
            break;
        }
        // Check if the current DFP supports DisplayPort
        LW0073_CTRL_DFP_GET_INFO_PARAMS dfpParams = {0};
        dfpParams.subDeviceInstance = subdev;
        dfpParams.displayId = (UINT32)(BIT(i));
        dfpParams.flags = 0;
        status = LwRmControl(m_hClient, m_hDisplay,
                         LW0073_CTRL_CMD_DFP_GET_INFO,
                         &dfpParams, sizeof (dfpParams));
        if (status != LW_OK)
        {
            Printf(Tee::PriNormal,
                "DpTest: Get DFP Info failed: 0x%x\n", status);
            return RmApiStatusToModsRC(status);
        }
        if(FLD_TEST_DRF(0073_CTRL_DFP, _FLAGS, _SIGNAL, _DISPLAYPORT, dfpParams.flags))
        {
            Printf(Tee::PriNormal,
                "DpTest: Found Display Port on displayId: 0x%x \n", BIT(i));

        }
        else
        {
            continue; //DFP supported, but is not a display port...continue
        }
        // This display has Display Port, continue with the checks here

        // Check for the Aux channel

        // Acquire Aux Channel Semaphore

        // Verify Aux Channel, through some simple reads (DPCP reads)

        // Do link training, set number of links/link rate

    }
    return rc;
}

RC DpTest::Run()
{
    RC rc = OK;
    LW0000_CTRL_GPU_GET_ATTACHED_IDS_PARAMS gpuAttachedIdsParams = {{0}};
    LW0000_CTRL_GPU_GET_ID_INFO_PARAMS gpuIdInfoParams = {0};
    LW0080_ALLOC_PARAMETERS lw0080Params = {0};
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
            "DpTest: GET_IDS failed: 0x%x\n", status);
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
                "DpTest: GET_ID_INFO failed: 0x%x\n", status);
            return RmApiStatusToModsRC(status);
        }

        if (maxDevice <= (UINT32)gpuIdInfoParams.deviceInstance)
        {
            maxDevice = 1+(UINT32)gpuIdInfoParams.deviceInstance;
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
                   "DpTest: LwRmAlloc(LW01_DEVICE_0) failed!\n");
            return RmApiStatusToModsRC(status);
        }

        m_isDeviceAllocated = true;

        // allocate LW04_DISPLAY_COMMON
        status = LwRmAlloc(m_hClient, m_hDev, m_hDisplay, LW04_DISPLAY_COMMON, NULL);
        if (status != LW_OK)
        {
            Printf(Tee::PriNormal,
                   "DpTest: LwRmAlloc(LW04_DISPLAY_COMMON) failed: "
                   "status 0x%x\n", status);
            return RmApiStatusToModsRC(status);
        }

        m_isDisplayAllocated = true;
//      CHECK_RC(DetectTest((UINT32)gpuIdInfoParams.subDeviceInstance));
//      CHECK_RC(EdidTest((UINT32)gpuIdInfoParams.subDeviceInstance));

        //
        // Detect if there are any Display Ports available on the current GPU
        // subdevice instance.
        //
        DetectDpInDFP((UINT32)gpuIdInfoParams.subDeviceInstance);

        // free handles allocated in this loop
        LwRmFree(m_hClient, m_hDev, m_hDisplay);
        m_isDisplayAllocated = false;
        LwRmFree(m_hClient, m_hClient, m_hDev);
        m_isDeviceAllocated = false;
    }
    return rc;
}

RC  DpTest::Cleanup()
{
    LwRmFree(m_hClient, m_hClient, m_hClient);
    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(DpTest, RmTest,
    "Detect and Edid Test:\n"
    "  This object contains 2 functionalities:\n"
    "   1. Detect and Edid Test: This test calls the RM to test \n"
    "      various ways of detection and it will atttempt to read \n"
    "      the EDID on all displayIDs.\n"
    "   2. Detect and Edid direct function calls: These calls allow\n"
    "      someone to directly call the Detection and Edid logic for\n"
    "      one gpu in the system and specify the displays or flags to"
    "      send to the RM.\n");

CLASS_PROP_READWRITE(DpTest, RunMode, UINT32,
    "Change the exelwtion of the Run() test: 0=Default, 1=Detect, 2=Edid.");
CLASS_PROP_READWRITE(DpTest, UserDevice, UINT32,
    "Device index to use when running Detect or Edid exclusively. "
    "Default is 0.");
CLASS_PROP_READWRITE(DpTest, UserSub, UINT32,
    "Subdevice index to use when running Detect or Edid exclusively. "
    "Default is 0.");
CLASS_PROP_READWRITE(DpTest, GetAuxPort, UINT32,
    "DisplayMask to use when running Detect or Edid exclusively. "
    "Default (0) is all possible displays.");
CLASS_PROP_READWRITE(DpTest, CtrlFlags, UINT32,
    "The direct RmControl flags to use for Detect or Edid exclusively. "
    "0 means use the default flags.");

