/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_gsync.cpp

#include "gpu/tests/rmtest.h"

#include "core/include/jscript.h"
#include "gpu/tests/gputestc.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "lwos.h"
#include "lwRmApi.h"

// RM CTRL util class
#include "gpu/utility/rmctrlutils.h"
#include "gpu/include/notifier.h"
#include "ctrl/ctrl0000.h" // GSYNC
#include "ctrl/ctrl0000/ctrl0000gsync.h" // Class LW30_GSYNC
#include "ctrl/ctrl30f1.h"               // GSYNC Ctrl Commands
#include "class/cl0073.h"
#include "ctrl/ctrl0073.h"
#include "ctrl/ctrl0080.h"
#include "core/include/memcheck.h"

// object handles
#define LWCTRL_DEVICE_HANDLE                (0xbb008000)
#define LWCTRL_GPU_DEVICE_HANDLE            (0xbb007300)
#define LWCTRL_DISPLAY_DEVICE_HANDLE        (0xbb009000)

// Parameters to the test TestGsyncControlParams()
#define NSYNC 0 // for every incomming pulse
#define SYNC_SKEW 1050 // 7.81uS*1050 = 8.2005 ms
#define SYNC_START_DELAY 1550 // 7.81uS*1550 = 12.1055 ms

class ClassGsyncTest : public RmTest
{
public:
    ClassGsyncTest();
    virtual ~ClassGsyncTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    LwRm::Handle GetDeviceHandle();
    bool IsGsyncDevicePresent();
    RC TestGsyncGetInfo();
    RC TestGsyncNullCmd();
    RC TestGsyncGetApiVersion();
    RC TestGsyncGetSyncStatus();
    RC TestGsyncGetStatusSignals();
    RC TestGsyncGetCaps();
    RC TestGsyncGetGpuTopology();
    RC TestGsyncControlSwapBarrier();
    RC TestGsyncGetControlSync();
    RC TestGsyncControlInterlaceMode();
    RC TestGsyncControlWatchDog();
    RC TestGsyncControlTesting();
    RC TestGsyncGetStatus();
    RC TestGsyncControlParams();
    void PrintGsyncGetControlSync(
        LW30F1_CTRL_GSYNC_GET_CONTROL_SYNC_PARAMS gsyncGetControlSyncParams);

    LwRm::Handle m_hClient;
    static const LwRm::Handle m_hGpuDevice = LWCTRL_GPU_DEVICE_HANDLE;
    static const LwRm::Handle m_hDisplay = LWCTRL_DISPLAY_DEVICE_HANDLE;

    LW0000_CTRL_GSYNC_GET_ATTACHED_IDS_PARAMS m_gsyncIdsTable;
};

//! \brief ClassGsyncTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
ClassGsyncTest::ClassGsyncTest()
{
    SetName("ClassGsyncTest");
    m_hClient = 0;
}

//! \brief ClassGsyncTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
ClassGsyncTest::~ClassGsyncTest()
{
}

//! \brief Whether or not the test is supported in the current environment.
//!
//! \return True if this is HW , False otherwise
//-----------------------------------------------------------------------------
string ClassGsyncTest::IsTestSupported()
{
    // Supported on hardware only
    if(Platform::Hardware != Platform::GetSimulationMode())
        return "Supported on HW only";

    return RUN_RMTEST_TRUE;
}

//! \brief Setup all necessary resources before running the test.
//!
//! \return RC -> OK if everything is allocated, test-specific RC if something
//!         failed while selwring resources.
//! \sa Run
//-----------------------------------------------------------------------------
RC ClassGsyncTest::Setup()
{
    RC rc = OK;
    UINT32 retVal;

    // Setup Client
    retVal = LwRmAllocRoot((LwU32*)&m_hClient);
    CHECK_RC(RmApiStatusToModsRC(retVal));

    return rc;
}

//
//! \brief Run the test!
//!
//! \return OK if the test passed, specific RC if it failed
//-----------------------------------------------------------------------------
RC ClassGsyncTest::Run()
{
    RC rc = OK;

    // Skip the test if device not present
    if(!IsGsyncDevicePresent())
    {
        Printf(Tee::PriHigh, "%s\n",
            "Gsync Device Not Present. Skipping Test.");
        return rc;
    }

    // Perform test for command LW0000_CTRL_CMD_GSYNC_GET_ID_INFO
    CHECK_RC(TestGsyncGetInfo());

    // Perform test for the command LW30F1_CTRL_CMD_NULL
    CHECK_RC(TestGsyncNullCmd());

    // Perform test for the command LW30F1_CTRL_CMD_GSYNC_GET_VERSION
    CHECK_RC(TestGsyncGetApiVersion());

    // Perform test for the command LW30F1_CTRL_CMD_GSYNC_GET_STATUS_SYNC
    CHECK_RC(TestGsyncGetSyncStatus());

    // Perform test for the command LW30F1_CTRL_CMD_GSYNC_GET_STATUS_SIGNALS
    CHECK_RC(TestGsyncGetStatusSignals());

    // Perform test for command LW30F1_CTRL_CMD_GSYNC_GET_CAPS
    CHECK_RC(TestGsyncGetCaps());

    // Perform test for command LW30F1_CTRL_CMD_GET_GSYNC_GPU_TOPOLOGY
    CHECK_RC(TestGsyncGetGpuTopology());

    // Perform test for command CONTROL_SWAP_BARRIER
    CHECK_RC(TestGsyncControlSwapBarrier());

    // Perform test for command LW30F1_CTRL_CMD_GSYNC_GET_CONTROL_SYNC
    CHECK_RC(TestGsyncGetControlSync());

    // Perform test for command CONTROL_INTERLACE_MODE
    CHECK_RC(TestGsyncControlInterlaceMode());

    // Perform test for command CONTROL_WATCHDOG
    CHECK_RC(TestGsyncControlWatchDog());

    // Perform test for command LW30F1_CTRL_CMD_GSYNC_GET_STATUS
    CHECK_RC(TestGsyncGetStatus());

    // Perform test for command CONTROL_TESTING
    CHECK_RC(TestGsyncControlTesting());

    // Perform test for commands  LW30F1_CTRL_GSYNC_SET_CONTROL_PARAMS_PARAMS
    // and LW30F1_CTRL_GSYNC_GET_CONTROL_PARAMS_PARAMS
    CHECK_RC(TestGsyncControlParams());

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! Cleanup all the allocated resources that the test selwred during Setup.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
RC ClassGsyncTest::Cleanup()
{
    // Free Client
    if (0 != m_hClient)
    {
        LwRmFree(m_hClient, m_hClient, m_hClient);
        m_hClient = 0;
    }

    return OK;
}

//-----------------------------------------------------------------------------
// Private Member Functions
//-----------------------------------------------------------------------------

//! \brief Checking for HW Presence
//!
//! \return true if at least one GSYNC device exists, false otherwise
//-----------------------------------------------------------------------------
bool ClassGsyncTest::IsGsyncDevicePresent()
{
    RC rc = OK;
    UINT32 retVal;
    UINT32 i;
    bool deviceFound = false;
    LW0000_CTRL_GSYNC_GET_ATTACHED_IDS_PARAMS gsyncAttachedIdsParams;

    // Get attached gpus ids
    memset(&gsyncAttachedIdsParams, 0, sizeof (gsyncAttachedIdsParams));
    retVal = LwRmControl(m_hClient,
        m_hClient,
        LW0000_CTRL_CMD_GSYNC_GET_ATTACHED_IDS,
        &gsyncAttachedIdsParams,
        sizeof (gsyncAttachedIdsParams));
    rc = RmApiStatusToModsRC(retVal);
    if(OK != rc)
    {
        Printf(Tee::PriHigh,"LW0000_CTRL_CMD_GSYNC_GET_ATTACHED_IDS failed : "
            "Cann't Retrieve Gsync Device List\n");
        return false;
    }

    Printf(Tee::PriHigh, "Printing Gsync Device Table : \n");
    for(i = 0; i < LW30F1_MAX_GSYNCS; i++)
    {
        Printf(Tee::PriHigh, "Id : 0x%x\n",
            (UINT32)gsyncAttachedIdsParams.gsyncIds[i]);
        if(LW0000_CTRL_GSYNC_ILWALID_ID != gsyncAttachedIdsParams.gsyncIds[i])
            deviceFound = true;
    }

    // Fill in the global m_gsyncIdsTable
    m_gsyncIdsTable = gsyncAttachedIdsParams;
    return deviceFound;
}

//! \brief Generates new device handles for gsync device
//!
//! \return Handle
//-----------------------------------------------------------------------------
LwRm::Handle ClassGsyncTest::GetDeviceHandle()
{
    static LwRm::Handle hDevice = LWCTRL_DEVICE_HANDLE;

    return hDevice++;
}

//! \brief Test for command LW0000_CTRL_CMD_GSYNC_GET_ID_INFO
//!
//! \return OK if the test passed, specific RC if it failed
//-----------------------------------------------------------------------------
RC ClassGsyncTest::TestGsyncGetInfo()
{
    RC     rc=OK;
    UINT32 retVal;
    UINT32 i;

    LW0000_CTRL_GSYNC_GET_ID_INFO_PARAMS    gsyncIdInfo = {0};
    Printf(Tee::PriHigh, "\nEntering TestGsyncGetInfo()\n");

    for(i = 0; i < LW30F1_MAX_GSYNCS; i++)
    {
        gsyncIdInfo.gsyncId = m_gsyncIdsTable.gsyncIds[i];

        // Checking retVal for valid devices
        if (LW0000_CTRL_GSYNC_ILWALID_ID != m_gsyncIdsTable.gsyncIds[i])
        {
            //Get Gsync Instance
            retVal = LwRmControl(m_hClient,
                                 m_hClient,
                                 LW0000_CTRL_CMD_GSYNC_GET_ID_INFO,
                                 &gsyncIdInfo,
                                 sizeof (gsyncIdInfo));
            rc = RmApiStatusToModsRC(retVal);

            switch (retVal)
            {
                case LW_OK:
                {
                    Printf(Tee::PriHigh,
                        "Info for Gsync Device with [Id:0x%x:]\n",
                           (UINT32)m_gsyncIdsTable.gsyncIds[i]);

                    Printf(Tee::PriHigh,
                           "gsyncFlags    : 0x%x\n",
                           (UINT32)gsyncIdInfo.gsyncFlags);

                    Printf(Tee::PriHigh,
                           "gsyncInstance : 0x%x\n",
                           (UINT32)gsyncIdInfo.gsyncInstance);
                    break;
                }

                case LW_ERR_ILWALID_PARAM_STRUCT:
                {
                    Printf(Tee::PriHigh,
                           "Invalid parameter structure for device [Id:0x%x]\n",
                           (UINT32)m_gsyncIdsTable.gsyncIds[i]);

                    return rc;
                }

                case LW_ERR_ILWALID_ARGUMENT:
                    {
                    Printf(Tee::PriHigh,
                           "Failed to get Info for device [Id: 0x%x]\n",
                           (UINT32)m_gsyncIdsTable.gsyncIds[i]);

                    return rc;
                    }
                default:
                    {
                        Printf(Tee::PriHigh,
                            "Giving unexpected return for device [Id:0x%x]\n",
                           (UINT32)m_gsyncIdsTable.gsyncIds[i]);
                        return rc;
                    }
            }
        }
        // Checking retVal for invalid devices
        else
        {
            return rc;
        }
    }

    return rc;
}

//! \brief Test for command LW30F1_CTRL_CMD_NULL
//!
//! \return OK if the test passed, specific RC if it failed
//-----------------------------------------------------------------------------
RC ClassGsyncTest::TestGsyncNullCmd()
{
    RC            rc=OK;
    UINT32        retVal;
    UINT32        i;
    LwRm::Handle  hDevice = 0;

    LW30F1_ALLOC_PARAMETERS                  Lw30f1AllocParams;
    LW0000_CTRL_GSYNC_GET_ID_INFO_PARAMS     gsyncIdInfoParams;

    Printf(Tee::PriHigh, "\nEntering TestGsyncNullCmd()\n");

    for(i = 0; i < LW30F1_MAX_GSYNCS; i++)
    {
        if(LW0000_CTRL_GSYNC_ILWALID_ID == m_gsyncIdsTable.gsyncIds[i])
            break;

        Printf(Tee::PriHigh, "Testing GSync Device [Id:0x%x:]\n",
               (UINT32)m_gsyncIdsTable.gsyncIds[i]);

        // Get Gsync Instance
        memset(&gsyncIdInfoParams, 0, sizeof (gsyncIdInfoParams));
        gsyncIdInfoParams.gsyncId = m_gsyncIdsTable.gsyncIds[i];

        retVal = LwRmControl(m_hClient,
                             m_hClient,
                             LW0000_CTRL_CMD_GSYNC_GET_ID_INFO,
                             &gsyncIdInfoParams,
                             sizeof (gsyncIdInfoParams));
        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
        {
            Printf(Tee::PriHigh,
                   "Error related to GET INFO cmd.\n");
        }
        CHECK_RC(rc);

        // Allocate GSYNC Device Object
        memset(&Lw30f1AllocParams, 0, sizeof (Lw30f1AllocParams));
        hDevice = GetDeviceHandle();
        Lw30f1AllocParams.gsyncInstance = gsyncIdInfoParams.gsyncInstance;

        retVal = LwRmAlloc(m_hClient, m_hClient, hDevice,
                           LW30_GSYNC,
                           &Lw30f1AllocParams);

        CHECK_RC(RmApiStatusToModsRC(retVal));

        // Send NULL command
        retVal = LwRmControl(m_hClient,
                             hDevice,
                             LW30F1_CTRL_CMD_NULL,
                             NULL,
                             0);
        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
        {
            Printf(Tee::PriHigh,
                   "Error related to NULL cmd.\n");
            // Free Device
            LwRmFree(m_hClient, m_hClient, hDevice);
            CHECK_RC(rc);
        }
        else
        {
             Printf(Tee::PriHigh,
                    "NULL command is working fine\n");
        }

        //Free Device
        retVal = LwRmFree(m_hClient, m_hClient, hDevice);
        CHECK_RC(RmApiStatusToModsRC(retVal));
    }
    return rc;
}

//! \brief Test for command LW30F1_CTRL_CMD_GSYNC_GET_VERSION
//!
//! \return OK if the test passed, specific RC if it failed
//-----------------------------------------------------------------------------
RC ClassGsyncTest::TestGsyncGetApiVersion()
{
    RC           rc=OK;
    UINT32       retVal;
    UINT32       i;
    LwRm::Handle hDevice = 0;

    LW30F1_ALLOC_PARAMETERS                  Lw30f1AllocParams;
    LW0000_CTRL_GSYNC_GET_ID_INFO_PARAMS     gsyncIdInfoParams;
    LW30F1_CTRL_GSYNC_GET_VERSION_PARAMS     gsyncApiVersionParams;

    Printf(Tee::PriHigh, "\nEntering TestGsyncGetApiVersion()\n");

    for(i = 0; i < LW30F1_MAX_GSYNCS; i++)
    {
        if(LW0000_CTRL_GSYNC_ILWALID_ID == m_gsyncIdsTable.gsyncIds[i])
            break;

        Printf(Tee::PriHigh, "Testing GSync Device [Id:0x%x:]\n",
               (UINT32)m_gsyncIdsTable.gsyncIds[i]);

        // Get Gsync Instance
        memset(&gsyncIdInfoParams, 0, sizeof (gsyncIdInfoParams));
        gsyncIdInfoParams.gsyncId = m_gsyncIdsTable.gsyncIds[i];

        retVal = LwRmControl(m_hClient,
                             m_hClient,
                             LW0000_CTRL_CMD_GSYNC_GET_ID_INFO,
                             &gsyncIdInfoParams,
                             sizeof (gsyncIdInfoParams));
        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
        {
            Printf(Tee::PriHigh,
                   "Error related to GET INFO cmd.\n");
        }
        CHECK_RC(rc);

        // Allocate GSYNC Device Object
        memset(&Lw30f1AllocParams, 0, sizeof (Lw30f1AllocParams));
        hDevice = GetDeviceHandle();
        Lw30f1AllocParams.gsyncInstance = gsyncIdInfoParams.gsyncInstance;

        retVal = LwRmAlloc(m_hClient, m_hClient, hDevice,
                           LW30_GSYNC,
                           &Lw30f1AllocParams);

        CHECK_RC(RmApiStatusToModsRC(retVal));

        // Get GSYNC Api Version Info.
        memset(&gsyncApiVersionParams, 0, sizeof (gsyncApiVersionParams));
        retVal = LwRmControl(m_hClient,
                             hDevice,
                             LW30F1_CTRL_CMD_GSYNC_GET_VERSION,
                             &gsyncApiVersionParams,
                             sizeof (gsyncApiVersionParams));
        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
        {
            Printf(Tee::PriHigh,
                   "Error related to GET VERSION cmd.\n");
            // Free Device
            LwRmFree(m_hClient, m_hClient, hDevice);
            CHECK_RC(rc);
        }

        // Print version Info.
        Printf(Tee::PriHigh,
               "Gsync API version Information :\n");
        Printf(Tee::PriHigh,
               "version  : %d\n",
               (UINT32)gsyncApiVersionParams.version);
        Printf(Tee::PriHigh,
               "revision : %d\n",
               (UINT32)gsyncApiVersionParams.revision);
        if(LW30F1_CTRL_GSYNC_API_VER != gsyncApiVersionParams.version)
        {
            Printf(Tee::PriHigh,
                   "Version  : %d is incorrect\n",
                   (UINT32)gsyncApiVersionParams.version);
        }
        if(LW30F1_CTRL_GSYNC_API_REV != gsyncApiVersionParams.revision)
        {
            Printf(Tee::PriHigh,
                "Revision : %d is incorrect\n",
                   (UINT32)gsyncApiVersionParams.revision);
        }

        //Free Device
        retVal = LwRmFree(m_hClient, m_hClient, hDevice);
        CHECK_RC(RmApiStatusToModsRC(retVal));
    }

    return rc;
}

//! \brief Test for command LW30F1_CTRL_CMD_GSYNC_GET_STATUS_SYNC
//!
//! \return OK if the test passed, specific RC if it failed
//-----------------------------------------------------------------------------
RC ClassGsyncTest::TestGsyncGetSyncStatus()
{
    RC            rc=OK;
    UINT32        retVal;
    LwRm::Handle  hDevice = 0;
    UINT32        i, j;

    LW30F1_ALLOC_PARAMETERS                    Lw30f1AllocParams;
    LW0000_CTRL_GSYNC_GET_ID_INFO_PARAMS       gsyncIdInfoParams;
    LW30F1_CTRL_GET_GSYNC_GPU_TOPOLOGY_PARAMS  gsyncGpuTopologyParams;
    LW30F1_CTRL_GSYNC_GET_STATUS_SYNC_PARAMS   gsyncSyncStatusParams;

    Printf(Tee::PriHigh, "\nEntering TestGsyncGetSyncStatus()\n");

    for(i = 0; i < LW30F1_MAX_GSYNCS; i++)
    {
        if(LW0000_CTRL_GSYNC_ILWALID_ID == m_gsyncIdsTable.gsyncIds[i])
            break;

        Printf(Tee::PriHigh, "Testing GSync Device [Id:0x%x:]\n",
               (UINT32)m_gsyncIdsTable.gsyncIds[i]);

        // Get Gsync Instance
        memset(&gsyncIdInfoParams, 0, sizeof (gsyncIdInfoParams));
        gsyncIdInfoParams.gsyncId = m_gsyncIdsTable.gsyncIds[i];

        retVal = LwRmControl(m_hClient,
                             m_hClient,
                             LW0000_CTRL_CMD_GSYNC_GET_ID_INFO,
                             &gsyncIdInfoParams,
                             sizeof (gsyncIdInfoParams));

        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
        {
            Printf(Tee::PriHigh,
                   "Error related to GET INFO cmd.\n");
            CHECK_RC(rc);
        }

        // Allocate GSYNC Device Object
        memset(&Lw30f1AllocParams, 0, sizeof (Lw30f1AllocParams));
        hDevice = GetDeviceHandle();
        Lw30f1AllocParams.gsyncInstance = gsyncIdInfoParams.gsyncInstance;

        retVal = LwRmAlloc(m_hClient, m_hClient, hDevice,
                           LW30_GSYNC,
                           &Lw30f1AllocParams);
        CHECK_RC(RmApiStatusToModsRC(retVal));

        // Get GPU TOPOLOGY
        memset(&gsyncGpuTopologyParams, 0, sizeof (gsyncGpuTopologyParams));

        retVal = LwRmControl(m_hClient,
                             hDevice,
                             LW30F1_CTRL_CMD_GET_GSYNC_GPU_TOPOLOGY,
                             &gsyncGpuTopologyParams,
                             sizeof (gsyncGpuTopologyParams));

        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
        {
            Printf(Tee::PriHigh,
                   "Error related to GET GPU TOPOLOGY cmd.\n");
            // Free Device
            LwRmFree(m_hClient, m_hClient, hDevice);
            CHECK_RC(rc);
        }

        // Print GPU TOPOLOGY
        Printf(Tee::PriHigh, "Printing Sync Status For Each Gpu :\n");
        for(j = 0; j < LW30F1_CTRL_MAX_GPUS_PER_GSYNC; j++)
        {
            if(LW30F1_CTRL_GPU_ILWALID_ID
                == gsyncGpuTopologyParams.gpus[j].gpuId)
                break;

            Printf(Tee::PriHigh, "Sync Status for GpuId : 0x%x\n",
                   (UINT32)gsyncGpuTopologyParams.gpus[j].gpuId);

            // Get Sync Status of attached GPUs
            memset(&gsyncSyncStatusParams, 0, sizeof (gsyncSyncStatusParams));
            gsyncSyncStatusParams.gpuId = gsyncGpuTopologyParams.gpus[j].gpuId;

            retVal = LwRmControl(m_hClient,
                                 hDevice,
                                 LW30F1_CTRL_CMD_GSYNC_GET_STATUS_SYNC,
                                 &gsyncSyncStatusParams,
                                 sizeof (gsyncSyncStatusParams));
            rc = RmApiStatusToModsRC(retVal);

            switch(retVal)
            {
                case LW_OK:
                {
                            Printf(Tee::PriHigh,
                                   "bTiming     : %d \n",
                                   (UINT32)gsyncSyncStatusParams.bTiming);
                            Printf(Tee::PriHigh,
                                   "bStereoSync : %d \n",
                                   (UINT32)gsyncSyncStatusParams.bStereoSync);
                            Printf(Tee::PriHigh,
                                   "bSyncReady  : %d \n",
                                   (UINT32)gsyncSyncStatusParams.bSyncReady);
                    break;
                }
                case LW_ERR_GENERIC:
                {
                    Printf(Tee::PriHigh,
                           "Generic Error related to GET STATUS SYNC cmd.\n");
                    break;
                }
                case LW_ERR_ILWALID_ARGUMENT:
                {
                    Printf(Tee::PriHigh,
                        "Error: Invalid Argumants to GET STATUS SYNC cmd.\n");
                    break;
                }
                default:
                {
                    Printf(Tee::PriHigh,
                        "Unexpected retVal = %d from rmControl Cmd\n", retVal);
                }
            }
            if(OK != rc)
            {
                // Free Device if error oclwrs in between
                LwRmFree(m_hClient, m_hClient, hDevice);
                CHECK_RC(rc);
            }
        }
        //Free Device
        retVal = LwRmFree(m_hClient, m_hClient, hDevice);
        CHECK_RC(RmApiStatusToModsRC(retVal));
    }

    return rc;
}

//! \brief Test for command LW30F1_CTRL_CMD_GSYNC_GET_STATUS_SIGNALS
//!
//! \return OK if the test passed, specific RC if it failed
//-----------------------------------------------------------------------------
RC ClassGsyncTest::TestGsyncGetStatusSignals()
{
    RC            rc=OK;
    UINT32        retVal;
    UINT32        i;
    LwRm::Handle  hDevice = 0;

    LW30F1_ALLOC_PARAMETERS                      Lw30f1AllocParams;
    LW0000_CTRL_GSYNC_GET_ID_INFO_PARAMS         gsyncIdInfoParams;
    LW30F1_CTRL_GSYNC_GET_STATUS_SIGNALS_PARAMS  gsyncStatusSignalParams;

    Printf(Tee::PriHigh, "\nEntering TestGsyncGetStatusSignals()\n");

    for(i = 0; i < LW30F1_MAX_GSYNCS; i++)
    {
        if(LW0000_CTRL_GSYNC_ILWALID_ID == m_gsyncIdsTable.gsyncIds[i])
            return rc;

        Printf(Tee::PriHigh, "Testing GSync Device [Id:0x%x:]\n",
               (UINT32)m_gsyncIdsTable.gsyncIds[i]);

        // Get Gsync Instance
        memset(&gsyncIdInfoParams, 0, sizeof (gsyncIdInfoParams));
        gsyncIdInfoParams.gsyncId = m_gsyncIdsTable.gsyncIds[i];

        retVal = LwRmControl(m_hClient,
                             m_hClient,
                             LW0000_CTRL_CMD_GSYNC_GET_ID_INFO,
                             &gsyncIdInfoParams,
                             sizeof (gsyncIdInfoParams));
        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
        {
            Printf(Tee::PriHigh,
                   "Error related to GET INFO cmd.\n");
        }
        CHECK_RC(rc);

        // Allocate GSYNC Device Object
        memset(&Lw30f1AllocParams, 0, sizeof (Lw30f1AllocParams));
        hDevice = GetDeviceHandle();
        Lw30f1AllocParams.gsyncInstance = gsyncIdInfoParams.gsyncInstance;

        retVal = LwRmAlloc(m_hClient, m_hClient, hDevice,
                           LW30_GSYNC,
                           &Lw30f1AllocParams);

        CHECK_RC(RmApiStatusToModsRC(retVal));

        // Get Gsync Status Signals
        memset(&gsyncStatusSignalParams, 0, sizeof (gsyncStatusSignalParams));
        retVal = LwRmControl(m_hClient,
                             hDevice,
                             LW30F1_CTRL_CMD_GSYNC_GET_STATUS_SIGNALS,
                             &gsyncStatusSignalParams,
                             sizeof(gsyncStatusSignalParams));
        rc = RmApiStatusToModsRC(retVal);

        Printf(Tee::PriHigh, "Printing Incoming Singal Status :\n");
        switch(retVal)
        {
            case LW_OK:
            {
                Printf(Tee::PriHigh,
                       "RJ45[0] : %d \n",
                       (UINT32)gsyncStatusSignalParams.RJ45[0]);
                Printf(Tee::PriHigh,
                       "RJ45[1] : %d \n",
                       (UINT32)gsyncStatusSignalParams.RJ45[1]);
                Printf(Tee::PriHigh,
                       "house   : %d \n",
                       (UINT32)gsyncStatusSignalParams.house);
                Printf(Tee::PriHigh,
                       "rate    : %d \n",
                       (UINT32)gsyncStatusSignalParams.rate);
                break;
            }
            case LW_ERR_ILWALID_ARGUMENT:
            {
                Printf(Tee::PriHigh,
                       "Invalid arguments to GET STATUS SIGNALS cmd.\n");
                break;
            }
            case LW_ERR_ILWALID_PARAM_STRUCT:
            {
                Printf(Tee::PriHigh,
                       "Invalid param structure to GET STATUS SIGNALS cmd.\n");
                break;
            }
            default:
            {
                Printf(Tee::PriHigh,
                       "gsyncGetStatusSignals() is not implemented\n");
                Printf(Tee::PriHigh,
                       "skipping... the test TestGsyncGetStatusSignals()\n");
                retVal = 0;
                rc = RmApiStatusToModsRC(retVal);
            }
        }
        retVal = LwRmFree(m_hClient, m_hClient, hDevice);
        CHECK_RC(RmApiStatusToModsRC(retVal));
    }
    return rc;
}

//! \brief Test for command LW30F1_CTRL_CMD_GSYNC_GET_CAPS
//!
//! \return OK if the test passed, specific RC if it failed
//-----------------------------------------------------------------------------
RC ClassGsyncTest::TestGsyncGetCaps()
{
    RC rc = OK;
    UINT32 retVal;
    LW30F1_ALLOC_PARAMETERS Lw30f1AllocParams;
    LW0000_CTRL_GSYNC_GET_ID_INFO_PARAMS gsyncIdInfoParams;
    LW30F1_CTRL_GSYNC_GET_CAPS_PARAMS gsyncGetCapsParams;
    static LwRm::Handle hDevice = 0;
    bool capFlagsMatch = false;
    UINT32 i;

    Printf(Tee::PriHigh, "Entering TestGsyncGetCaps()\n");

    for(i = 0; i < LW30F1_MAX_GSYNCS; i++)
    {
        if(LW0000_CTRL_GSYNC_ILWALID_ID == m_gsyncIdsTable.gsyncIds[i])
            break;

        Printf(Tee::PriHigh, "Testing GSync Device [Id:0x%x:]\n",
            (UINT32)m_gsyncIdsTable.gsyncIds[i]);

        // Get Gsync Instance
        memset(&gsyncIdInfoParams, 0, sizeof (gsyncIdInfoParams));
        gsyncIdInfoParams.gsyncId = m_gsyncIdsTable.gsyncIds[i];
        retVal = LwRmControl(m_hClient,
            m_hClient,
            LW0000_CTRL_CMD_GSYNC_GET_ID_INFO,
            &gsyncIdInfoParams,
            sizeof (gsyncIdInfoParams));
        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
            Printf(Tee::PriHigh,
                "Error in call LW0000_CTRL_CMD_GSYNC_GET_ID_INFO.\n");
        CHECK_RC(rc);

        // Allocate GSYNC Device Object
        memset(&Lw30f1AllocParams, 0, sizeof (Lw30f1AllocParams));
        hDevice = GetDeviceHandle();
        Lw30f1AllocParams.gsyncInstance = gsyncIdInfoParams.gsyncInstance;
        retVal = LwRmAlloc(m_hClient, m_hClient, hDevice,
            LW30_GSYNC,
            &Lw30f1AllocParams);
        CHECK_RC(RmApiStatusToModsRC(retVal));

        // Get GSYNC Caps
        memset(&gsyncGetCapsParams, 0, sizeof (gsyncGetCapsParams));
        retVal = LwRmControl(m_hClient,
            hDevice,
            LW30F1_CTRL_CMD_GSYNC_GET_CAPS,
            &gsyncGetCapsParams,
            sizeof (gsyncGetCapsParams));
        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
        {
            Printf(Tee::PriHigh,
                "Error in call LW30F1_CTRL_CMD_GSYNC_GET_CAPS.\n");
            // Free Device
            LwRmFree(m_hClient, m_hClient, hDevice);
        }
        CHECK_RC(rc);

        // Print CAPS
        Printf(Tee::PriHigh, "revId    : 0x%x\n",
            (UINT32)gsyncGetCapsParams.revId);
        // Validate and PrintboardId
        switch(gsyncGetCapsParams.boardId)
        {
        case LW30F1_CTRL_GSYNC_GET_CAPS_BOARD_ID_P2060:
            Printf(Tee::PriHigh, "boardId  : 0x%x [%s]\n",
                (UINT32)gsyncGetCapsParams.boardId, "BOARD_ID_P2060");
            break;

        default:
            Printf(Tee::PriHigh, "boardId  : 0x%x\n",
                (UINT32)gsyncGetCapsParams.boardId);
            Printf(Tee::PriHigh, "Invalid boardId value.\n");
            rc = RC::SOFTWARE_ERROR;
        }

        Printf(Tee::PriHigh, "revision : 0x%x\n",
            (UINT32)gsyncGetCapsParams.revision);

        // Print and Validate capFlags
        Printf(Tee::PriHigh, "capFlags : 0x%x\n",
            (UINT32)gsyncGetCapsParams.capFlags);
        if(gsyncGetCapsParams.capFlags
            & LW30F1_CTRL_GSYNC_GET_CAPS_CAP_FLAGS_FREQ_ACLWRACY_2DPS)
        {
            Printf(Tee::PriHigh, "capFlags : [%s]\n",
                "CAP_FLAGS_FREQ_ACLWRACY_2DPS");
            capFlagsMatch = true;
        }
        if(gsyncGetCapsParams.capFlags
            & LW30F1_CTRL_GSYNC_GET_CAPS_CAP_FLAGS_FREQ_ACLWRACY_3DPS)
        {
            Printf(Tee::PriHigh, "capFlags : [%s]\n",
                "CAP_FLAGS_FREQ_ACLWRACY_3DPS");
            capFlagsMatch = true;
        }
        if(gsyncGetCapsParams.capFlags
            & LW30F1_CTRL_GSYNC_GET_CAPS_CAP_FLAGS_FREQ_ACLWRACY_4DPS)
        {
            Printf(Tee::PriHigh, "capFlags : [%s]\n",
                "CAP_FLAGS_FREQ_ACLWRACY_4DPS");
            capFlagsMatch = true;
        }
        if(gsyncGetCapsParams.capFlags
            & LW30F1_CTRL_GSYNC_GET_CAPS_CAP_FLAGS_SYNC_LOCK_EVENT)
        {
            Printf(Tee::PriHigh, "capFlags : [%s]\n",
                "CAP_FLAGS_SYNC_LOCK_EVENT");
            capFlagsMatch = true;
        }
        if(gsyncGetCapsParams.capFlags
            & LW30F1_CTRL_GSYNC_GET_CAPS_CAP_FLAGS_HOUSE_SYNC_EVENT)
        {
            Printf(Tee::PriHigh, "capFlags : [%s]\n",
                "CAP_FLAGS_HOUSE_SYNC_EVENT");
            capFlagsMatch = true;
        }
        if(gsyncGetCapsParams.capFlags
            & LW30F1_CTRL_GSYNC_GET_CAPS_CAP_FLAGS_FRAME_COUNT_EVENT)
        {
            Printf(Tee::PriHigh, "capFlags : [%s]\n",
                "CAP_FLAGS_FRAME_COUNT_EVENT");
            capFlagsMatch = true;
        }

        if(!capFlagsMatch)
        {
            Printf(Tee::PriHigh, "Invalid capFlags value.\n");
            rc = RC::SOFTWARE_ERROR;
        }

        // Hardware Specific Validation
        if(gsyncGetCapsParams.capFlags
            & LW30F1_CTRL_GSYNC_GET_CAPS_CAP_FLAGS_FREQ_ACLWRACY_2DPS)
        {
            switch(gsyncGetCapsParams.boardId)
            {
            case LW30F1_CTRL_GSYNC_GET_CAPS_BOARD_ID_P2060:
                break;

            default:
                Printf(Tee::PriHigh, "capsFlags not matching with boardId\n");
                rc = RC::SOFTWARE_ERROR;
            }
        }

        //Free Device
        retVal = LwRmFree(m_hClient, m_hClient, hDevice);
        CHECK_RC(RmApiStatusToModsRC(retVal));
    }

    return rc;
}

//! \brief Test for command LW30F1_CTRL_CMD_GET_GSYNC_GPU_TOPOLOGY
//!
//! \return OK if the test passed, specific RC if it failed
//-----------------------------------------------------------------------------
RC ClassGsyncTest::TestGsyncGetGpuTopology()
{
    RC rc = OK;
    UINT32 retVal;
    LW30F1_ALLOC_PARAMETERS Lw30f1AllocParams;
    LW0000_CTRL_GSYNC_GET_ID_INFO_PARAMS gsyncIdInfoParams;
    LW30F1_CTRL_GET_GSYNC_GPU_TOPOLOGY_PARAMS gsyncGpuTopologyParams;
    LwRm::Handle hDevice = 0;
    UINT32 i, j;

    Printf(Tee::PriHigh, "Entering TestGsyncGetGpuTopology()\n");

    for(i = 0; i < LW30F1_MAX_GSYNCS; i++)
    {
        if(LW0000_CTRL_GSYNC_ILWALID_ID == m_gsyncIdsTable.gsyncIds[i])
            break;

        Printf(Tee::PriHigh, "Testing GSync Device [Id:0x%x:]\n",
            (UINT32)m_gsyncIdsTable.gsyncIds[i]);

        // Get Gsync Instance
        memset(&gsyncIdInfoParams, 0, sizeof (gsyncIdInfoParams));
        gsyncIdInfoParams.gsyncId = m_gsyncIdsTable.gsyncIds[i];
        retVal = LwRmControl(m_hClient,
            m_hClient,
            LW0000_CTRL_CMD_GSYNC_GET_ID_INFO,
            &gsyncIdInfoParams,
            sizeof (gsyncIdInfoParams));
        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
            Printf(Tee::PriHigh,
                "Error in call LW0000_CTRL_CMD_GSYNC_GET_ID_INFO.\n");
        CHECK_RC(rc);

        // Allocate GSYNC Device Object
        memset(&Lw30f1AllocParams, 0, sizeof (Lw30f1AllocParams));
        hDevice = GetDeviceHandle();
        Lw30f1AllocParams.gsyncInstance = gsyncIdInfoParams.gsyncInstance;
        retVal = LwRmAlloc(m_hClient, m_hClient, hDevice,
            LW30_GSYNC,
            &Lw30f1AllocParams);
        CHECK_RC(RmApiStatusToModsRC(retVal));

        // Get GPU TOPOLOGY
        memset(&gsyncGpuTopologyParams, 0, sizeof (gsyncGpuTopologyParams));
        retVal = LwRmControl(m_hClient,
            hDevice,
            LW30F1_CTRL_CMD_GET_GSYNC_GPU_TOPOLOGY,
            &gsyncGpuTopologyParams,
            sizeof (gsyncGpuTopologyParams));
        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
        {
            Printf(Tee::PriHigh,
                "Error in call LW30F1_CTRL_CMD_GET_GSYNC_GPU_TOPOLOGY.\n");
            // Free Device
            LwRmFree(m_hClient, m_hClient, hDevice);
        }
        CHECK_RC(rc);

        // Print GPU TOPOLOGY
        Printf(Tee::PriHigh, "Printing Gpu Topology For Each Gpu :\n");
        for(j = 0; j < LW30F1_CTRL_MAX_GPUS_PER_GSYNC; j++)
        {
            if(LW30F1_CTRL_GPU_ILWALID_ID
                == gsyncGpuTopologyParams.gpus[j].gpuId)
                break;

            Printf(Tee::PriHigh, "GpuId    : 0x%x\n",
                (UINT32)gsyncGpuTopologyParams.gpus[j].gpuId);
            if(LW30F1_CTRL_GET_GSYNC_GPU_TOPOLOGY_PRIMARY
                == gsyncGpuTopologyParams.gpus[j].connector)
                Printf(Tee::PriHigh, "Topology : [%s]\n",
                    "PRIMARY");
            else if(LW30F1_CTRL_GET_GSYNC_GPU_TOPOLOGY_SECONDARY
                == gsyncGpuTopologyParams.gpus[j].connector)
                Printf(Tee::PriHigh, "Topology : [%s]\n",
                    "SECONDARY");
            else
            {
                Printf(Tee::PriHigh, "Invalid Topology Value : 0x%0x\n",
                    (UINT32)gsyncGpuTopologyParams.gpus[j].connector);
                rc = RC::SOFTWARE_ERROR;
            }
        }

        //Free Device
        LwRmFree(m_hClient, m_hClient, hDevice);
        CHECK_RC(rc);
    }

    return rc;
}

//! \brief Test for command LW30F1_CTRL_CMD_GSYNC_GET_CONTROL_SWAP_BARRIER
//! and LW30F1_CTRL_CMD_GSYNC_SET_CONTROL_SWAP_BARRIER
//!
//! \return OK if the test passed, specific RC if it failed
//-----------------------------------------------------------------------------
RC ClassGsyncTest::TestGsyncControlSwapBarrier()
{
    RC rc = OK;
    UINT32 retVal;
    LW30F1_ALLOC_PARAMETERS Lw30f1AllocParams;
    LW0000_CTRL_GSYNC_GET_ID_INFO_PARAMS gsyncIdInfoParams;
    LW30F1_CTRL_GET_GSYNC_GPU_TOPOLOGY_PARAMS gsyncGpuTopologyParams;
    LW30F1_CTRL_GSYNC_GET_CONTROL_SWAP_BARRIER_PARAMS
        getControlSwapBarrierParams;
    LW30F1_CTRL_GSYNC_SET_CONTROL_SWAP_BARRIER_PARAMS
        setControlSwapBarrierParams;
    LwRm::Handle hDevice = 0;
    UINT32 i, j;

    Printf(Tee::PriHigh, "Entering TestGsyncGetControlSwapBarrier()\n");

    for(i = 0; i < LW30F1_MAX_GSYNCS; i++)
    {
        if(LW0000_CTRL_GSYNC_ILWALID_ID == m_gsyncIdsTable.gsyncIds[i])
            break;

        Printf(Tee::PriHigh, "Testing GSync Device [Id:0x%x:]\n",
            (UINT32)m_gsyncIdsTable.gsyncIds[i]);

        // Get Gsync Instance
        memset(&gsyncIdInfoParams, 0, sizeof (gsyncIdInfoParams));
        gsyncIdInfoParams.gsyncId = m_gsyncIdsTable.gsyncIds[i];
        retVal = LwRmControl(m_hClient,
            m_hClient,
            LW0000_CTRL_CMD_GSYNC_GET_ID_INFO,
            &gsyncIdInfoParams,
            sizeof (gsyncIdInfoParams));
        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
            Printf(Tee::PriHigh,
                "Error in call LW0000_CTRL_CMD_GSYNC_GET_ID_INFO.\n");
        CHECK_RC(rc);

        // Allocate GSYNC Device Object
        memset(&Lw30f1AllocParams, 0, sizeof (Lw30f1AllocParams));
        hDevice = GetDeviceHandle();
        Lw30f1AllocParams.gsyncInstance = gsyncIdInfoParams.gsyncInstance;
        retVal = LwRmAlloc(m_hClient, m_hClient, hDevice,
            LW30_GSYNC,
            &Lw30f1AllocParams);
        CHECK_RC(RmApiStatusToModsRC(retVal));

        // Get Gpu Ids
        memset(&gsyncGpuTopologyParams, 0, sizeof (gsyncGpuTopologyParams));
        retVal = LwRmControl(m_hClient,
            hDevice,
            LW30F1_CTRL_CMD_GET_GSYNC_GPU_TOPOLOGY,
            &gsyncGpuTopologyParams,
            sizeof (gsyncGpuTopologyParams));
        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
        {
            Printf(Tee::PriHigh,
                "Error in call LW30F1_CTRL_CMD_GET_GSYNC_GPU_TOPOLOGY.\n");
            // Free Device
            LwRmFree(m_hClient, m_hClient, hDevice);
        }
        CHECK_RC(rc);

        Printf(Tee::PriHigh,
            "Printing Control Swap Barrier Status For Each Gpu :\n");
        for(j = 0; j < LW30F1_CTRL_MAX_GPUS_PER_GSYNC; j++)
        {
            if(LW30F1_CTRL_GPU_ILWALID_ID
                == gsyncGpuTopologyParams.gpus[j].gpuId)
                break;

            Printf(Tee::PriHigh, "GpuId : 0x%x\n",
                (UINT32)gsyncGpuTopologyParams.gpus[j].gpuId);

            // Get Gpu Swap Barrier Status
            memset(&getControlSwapBarrierParams, 0,
                sizeof (getControlSwapBarrierParams));
            getControlSwapBarrierParams.gpuId =
                gsyncGpuTopologyParams.gpus[j].gpuId;
            retVal = LwRmControl(m_hClient,
                hDevice,
                LW30F1_CTRL_CMD_GSYNC_GET_CONTROL_SWAP_BARRIER,
                &getControlSwapBarrierParams,
                sizeof (getControlSwapBarrierParams));
            rc = RmApiStatusToModsRC(retVal);
            if(OK != rc)
            {
                Printf(Tee::PriHigh, "Error in call GetSwapBarrier.\n");
                // Free Device
                LwRmFree(m_hClient, m_hClient, hDevice);
            }
            CHECK_RC(rc);

            // Print Gpu Swap Barrier Status
            Printf(Tee::PriHigh, "Enable : 0x%x\n",
                (UINT32)getControlSwapBarrierParams.enable);

            // Toggle Gpu Swap Barrier Status
            memset(&setControlSwapBarrierParams, 0,
                sizeof (setControlSwapBarrierParams));
            setControlSwapBarrierParams.gpuId =
                gsyncGpuTopologyParams.gpus[j].gpuId;
            if(getControlSwapBarrierParams.enable)
                setControlSwapBarrierParams.enable = 0;
            else
                setControlSwapBarrierParams.enable = 1;
            retVal = LwRmControl(m_hClient,
                hDevice,
                LW30F1_CTRL_CMD_GSYNC_SET_CONTROL_SWAP_BARRIER,
                &setControlSwapBarrierParams,
                sizeof (setControlSwapBarrierParams));
            rc = RmApiStatusToModsRC(retVal);
            if(OK != rc)
            {
                Printf(Tee::PriHigh, "Error in call SetSwapBarrier.\n");
                // Free Device
                LwRmFree(m_hClient, m_hClient, hDevice);
            }
            CHECK_RC(rc);

            // Reread Gpu Swap Barrier Status
            memset(&getControlSwapBarrierParams, 0,
                sizeof (getControlSwapBarrierParams));
            getControlSwapBarrierParams.gpuId =
                gsyncGpuTopologyParams.gpus[j].gpuId;
            retVal = LwRmControl(m_hClient,
                hDevice,
                LW30F1_CTRL_CMD_GSYNC_GET_CONTROL_SWAP_BARRIER,
                &getControlSwapBarrierParams,
                sizeof (getControlSwapBarrierParams));
            rc = RmApiStatusToModsRC(retVal);
            if(OK != rc)
            {
                Printf(Tee::PriHigh, "Error in call GetSwapBarrier.\n");
                // Free Device
                LwRmFree(m_hClient, m_hClient, hDevice);
            }
            CHECK_RC(rc);

            // Print Gpu Swap Barrier Status After Toggle
            Printf(Tee::PriHigh, "Enable After Toggle : 0x%x\n",
                (UINT32)getControlSwapBarrierParams.enable);

            //Validate
            if(getControlSwapBarrierParams.enable !=
                setControlSwapBarrierParams.enable)
            {
                Printf(Tee::PriHigh, "Unexpected behaviour in SWAP_BARRIER.\n");
                rc = RC::SOFTWARE_ERROR;
                //Free Device
                LwRmFree(m_hClient, m_hClient, hDevice);
            }
        }
        CHECK_RC(rc);

        // Ilwoke Swap Barrier For Invalid Gup Id
        memset(&getControlSwapBarrierParams, 0,
            sizeof (getControlSwapBarrierParams));
        getControlSwapBarrierParams.gpuId = LW30F1_CTRL_GPU_ILWALID_ID;
        retVal = LwRmControl(m_hClient,
            hDevice,
            LW30F1_CTRL_CMD_GSYNC_GET_CONTROL_SWAP_BARRIER,
            &getControlSwapBarrierParams,
            sizeof (getControlSwapBarrierParams));
        rc = RmApiStatusToModsRC(retVal);
        if(OK == rc)
        {
            Printf(Tee::PriHigh, "Error in calling GetSwapBarrier"
                "with Invalid Gpu Id.\n");
             rc = RC::SOFTWARE_ERROR;
            // Free Device
            LwRmFree(m_hClient, m_hClient, hDevice);
        }
        else
            rc = OK;

        // Print Gpu Swap Barrier Status
        Printf(Tee::PriHigh, "Enable : 0x%x\n",
            (UINT32)getControlSwapBarrierParams.enable);

        //Free Device
        LwRmFree(m_hClient, m_hClient, hDevice);
        CHECK_RC(rc);
    }

    return rc;
}

//! \brief Test for command LW30F1_CTRL_CMD_GSYNC_GET_CONTROL_SYNC
//!
//! \return OK if the test passed, specific RC if it failed
//-----------------------------------------------------------------------------
RC ClassGsyncTest::TestGsyncGetControlSync()
{
    RC rc = OK;
    UINT32 retVal;
    LW30F1_ALLOC_PARAMETERS Lw30f1AllocParams;
    LW0000_CTRL_GSYNC_GET_ID_INFO_PARAMS gsyncIdInfoParams;
    LW30F1_CTRL_GET_GSYNC_GPU_TOPOLOGY_PARAMS gsyncGpuTopologyParams;
    LW0000_CTRL_GPU_GET_ID_INFO_PARAMS gpuIdInfoParams = {0};
    LW0080_ALLOC_PARAMETERS lw0080Params = {0};
    LW0080_CTRL_GPU_GET_NUM_SUBDEVICES_PARAMS numSubDevicesParams = {0};
    LW0073_CTRL_SYSTEM_GET_SUPPORTED_PARAMS dispSupportedParams = {0};
    LW0073_CTRL_SYSTEM_GET_CONNECT_STATE_PARAMS dispConnectParams;
    LW30F1_CTRL_GSYNC_GET_CONTROL_SYNC_PARAMS gsyncGetControlSyncParams;
    LwRm::Handle hDevice = 0;
    LwRm::Handle hGpuDev = 0;
    UINT32 i, j, subdev, dispMask;

    Printf(Tee::PriHigh, "Entering TestGsyncGetControlSync()\n");

    for(i = 0; i < LW30F1_MAX_GSYNCS; i++)
    {
        if(LW0000_CTRL_GSYNC_ILWALID_ID == m_gsyncIdsTable.gsyncIds[i])
            break;

        Printf(Tee::PriHigh, "Testing GSync Device [Id:0x%x:]\n",
            (UINT32)m_gsyncIdsTable.gsyncIds[i]);

        // Get Gsync Instance
        memset(&gsyncIdInfoParams, 0, sizeof (gsyncIdInfoParams));
        gsyncIdInfoParams.gsyncId = m_gsyncIdsTable.gsyncIds[i];
        retVal = LwRmControl(m_hClient,
            m_hClient,
            LW0000_CTRL_CMD_GSYNC_GET_ID_INFO,
            &gsyncIdInfoParams,
            sizeof (gsyncIdInfoParams));
        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
            Printf(Tee::PriHigh,
                "Error in call LW0000_CTRL_CMD_GSYNC_GET_ID_INFO.\n");
        CHECK_RC(rc);

        // Allocate GSYNC Device Object
        memset(&Lw30f1AllocParams, 0, sizeof (Lw30f1AllocParams));
        hDevice = GetDeviceHandle();
        Lw30f1AllocParams.gsyncInstance = gsyncIdInfoParams.gsyncInstance;
        retVal = LwRmAlloc(m_hClient, m_hClient, hDevice,
            LW30_GSYNC,
            &Lw30f1AllocParams);
        CHECK_RC(RmApiStatusToModsRC(retVal));

        // Get GPU Ids
        memset(&gsyncGpuTopologyParams, 0, sizeof (gsyncGpuTopologyParams));
        retVal = LwRmControl(m_hClient,
            hDevice,
            LW30F1_CTRL_CMD_GET_GSYNC_GPU_TOPOLOGY,
            &gsyncGpuTopologyParams,
            sizeof (gsyncGpuTopologyParams));
        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
        {
            Printf(Tee::PriHigh,
                "Error in call LW30F1_CTRL_CMD_GET_GSYNC_GPU_TOPOLOGY.\n");
            // Free Device
            LwRmFree(m_hClient, m_hClient, hDevice);
        }
        CHECK_RC(rc);

        // For each GPU
        Printf(Tee::PriHigh, "Printing Display Mask For Each Gpu :\n");
        gpuIdInfoParams.szName = (LwP64)NULL;
        for(j = 0; j < LW30F1_CTRL_MAX_GPUS_PER_GSYNC; j++)
        {
            if(LW30F1_CTRL_GPU_ILWALID_ID
                == gsyncGpuTopologyParams.gpus[j].gpuId)
                break;

            Printf(Tee::PriHigh, "GpuId    : 0x%x\n",
                (UINT32)gsyncGpuTopologyParams.gpus[j].gpuId);

            // get gpu instance info
            gpuIdInfoParams.gpuId = gsyncGpuTopologyParams.gpus[j].gpuId;
            retVal = LwRmControl(m_hClient, m_hClient,
                                LW0000_CTRL_CMD_GPU_GET_ID_INFO,
                                &gpuIdInfoParams, sizeof (gpuIdInfoParams));
            rc = RmApiStatusToModsRC(retVal);
            if(OK != rc)
            {
                Printf(Tee::PriHigh,
                    "Error in call LW0000_CTRL_CMD_GPU_GET_ID_INFO.\n");
                // Free Device
                LwRmFree(m_hClient, m_hClient, hDevice);
            }
            CHECK_RC(rc);

            // allocate the gpu's device
            lw0080Params.deviceId = gpuIdInfoParams.deviceInstance;
            lw0080Params.hClientShare = 0;
            hGpuDev = m_hGpuDevice + gpuIdInfoParams.deviceInstance;
            retVal = LwRmAlloc(m_hClient, m_hClient, hGpuDev,
                            LW01_DEVICE_0,
                            &lw0080Params);
            rc = RmApiStatusToModsRC(retVal);
            if(OK != rc)
            {
                Printf(Tee::PriHigh, "LwRmAlloc(LW01_DEVICE_0) failed!\n");
                // Free Device
                LwRmFree(m_hClient, m_hClient, hDevice);
            }
            CHECK_RC(rc);

            // allocate LW04_DISPLAY_COMMON
            retVal = LwRmAlloc(m_hClient, hGpuDev, m_hDisplay,
                LW04_DISPLAY_COMMON, NULL);
            rc = RmApiStatusToModsRC(retVal);
            if(OK != rc)
            {
                Printf(Tee::PriHigh, "LwRmAlloc(LW04_DISPLAY_COMMON) failed: ");
                // Free Devices
                LwRmFree(m_hClient, m_hClient, hGpuDev);
                LwRmFree(m_hClient, m_hClient, hDevice);
            }
            CHECK_RC(rc);

            // get number of subdevices
            retVal = LwRmControl(m_hClient, hGpuDev ,
                                 LW0080_CTRL_CMD_GPU_GET_NUM_SUBDEVICES,
                                 &numSubDevicesParams,
                                 sizeof (numSubDevicesParams));
            rc = RmApiStatusToModsRC(retVal);
            if(OK != rc)
            {
                Printf(Tee::PriHigh, "Subdevice cnt cmd failed\n");
                // Free Devices
                LwRmFree(m_hClient, hGpuDev, m_hDisplay);
                LwRmFree(m_hClient, m_hClient, hGpuDev);
                LwRmFree(m_hClient, m_hClient, hDevice);
            }
            CHECK_RC(rc);

            //Print Number of Subdevices
            Printf(Tee::PriHigh,
                "Number of Subdevices : 0x%x\n",
                (UINT32)numSubDevicesParams.numSubDevices);

            // for each subdevice...
            for (subdev = 0; subdev < numSubDevicesParams.numSubDevices;
                subdev++)
            {
                Printf(Tee::PriHigh, "Testing Subdevice : 0x%x\n",
                    (UINT32)subdev);

                // get supported displays
                dispSupportedParams.subDeviceInstance = subdev;
                dispSupportedParams.displayMask = 0;
                retVal = LwRmControl(m_hClient, m_hDisplay,
                    LW0073_CTRL_CMD_SYSTEM_GET_SUPPORTED,
                    &dispSupportedParams, sizeof (dispSupportedParams));
                rc = RmApiStatusToModsRC(retVal);
                if(OK != rc)
                {
                    Printf(Tee::PriHigh, "Displays supported failed\n");
                    // Free Devices
                    LwRmFree(m_hClient, hGpuDev, m_hDisplay);
                    LwRmFree(m_hClient, m_hClient, hGpuDev);
                    LwRmFree(m_hClient, m_hClient, hDevice);
                }
                CHECK_RC(rc);

                dispMask = dispSupportedParams.displayMask;
                Printf(Tee::PriHigh, "Supported Display Mask: 0x%x\n",
                    (UINT32)dispMask);

                // get connected displays
                memset(&dispConnectParams, 0, sizeof(dispConnectParams));
                dispConnectParams.subDeviceInstance =
                    gpuIdInfoParams.subDeviceInstance;
                dispConnectParams.flags = 0;
                retVal = LwRmControl(m_hClient, m_hDisplay,
                    LW0073_CTRL_CMD_SYSTEM_GET_CONNECT_STATE,
                    &dispConnectParams, sizeof (dispConnectParams));
                rc = RmApiStatusToModsRC(retVal);
                if(OK != rc)
                {
                    Printf(Tee::PriHigh, "Connect state failed\n");
                    // Free Devices
                    LwRmFree(m_hClient, hGpuDev, m_hDisplay);
                    LwRmFree(m_hClient, m_hClient, hGpuDev);
                    LwRmFree(m_hClient, m_hClient, hDevice);
                }
                CHECK_RC(rc);

                //dispMask = dispConnectParams.displayMask;
                Printf(Tee::PriHigh, "Connected Display Mask: 0x%x\n",
                    (UINT32)dispConnectParams.displayMask);

                // Call LW30F1_CTRL_CMD_GSYNC_GET_CONTROL_SYNC : Case 1
                Printf(Tee::PriHigh, "Testing Get Control Sync :"
                    "(Case 1):\n");
                memset(&gsyncGetControlSyncParams, 0,
                    sizeof (gsyncGetControlSyncParams));
                gsyncGetControlSyncParams.gpuId =
                    gsyncGpuTopologyParams.gpus[j].gpuId;
                retVal = LwRmControl(m_hClient, hDevice,
                                    LW30F1_CTRL_CMD_GSYNC_GET_CONTROL_SYNC,
                                    &gsyncGetControlSyncParams,
                                    sizeof (gsyncGetControlSyncParams));
                rc = RmApiStatusToModsRC(retVal);
                if(OK != rc)
                {
                    Printf(Tee::PriHigh, "Gsync get control sync failed\n");
                    // Free Devices
                    LwRmFree(m_hClient, hGpuDev, m_hDisplay);
                    LwRmFree(m_hClient, m_hClient, hGpuDev);
                    LwRmFree(m_hClient, m_hClient, hDevice);
                }
                CHECK_RC(rc);

                PrintGsyncGetControlSync(gsyncGetControlSyncParams);

                // Call LW30F1_CTRL_CMD_GSYNC_GET_CONTROL_SYNC : Case 2
                Printf(Tee::PriHigh, "Testing Get Control Sync :"
                    "(Case 2):\n");
                memset(&gsyncGetControlSyncParams, 0,
                    sizeof (gsyncGetControlSyncParams));
                gsyncGetControlSyncParams.master = 1;
                gsyncGetControlSyncParams.gpuId =
                    gsyncGpuTopologyParams.gpus[j].gpuId;
                retVal = LwRmControl(m_hClient, hDevice,
                    LW30F1_CTRL_CMD_GSYNC_GET_CONTROL_SYNC,
                    &gsyncGetControlSyncParams,
                    sizeof (gsyncGetControlSyncParams));
                rc = RmApiStatusToModsRC(retVal);
                if(OK != rc)
                {
                    Printf(Tee::PriHigh, "Gsync get control sync failed\n");
                    // Free Devices
                    LwRmFree(m_hClient, hGpuDev, m_hDisplay);
                    LwRmFree(m_hClient, m_hClient, hGpuDev);
                    LwRmFree(m_hClient, m_hClient, hDevice);
                }
                CHECK_RC(rc);

                PrintGsyncGetControlSync(gsyncGetControlSyncParams);

                // Call LW30F1_CTRL_CMD_GSYNC_GET_CONTROL_SYNC : Case 3
                Printf(Tee::PriHigh, "Testing Get Control Sync :"
                    "(Case 3):\n");
                // for each display...
                for (i = 0; i < sizeof (dispMask); i++)
                {
                    if (0 == ((1 << i) & dispMask))
                        continue;

                    Printf(Tee::PriHigh, "DisplayId : 0x%x\n",
                        (UINT32)(1 << i));
                    memset(&gsyncGetControlSyncParams, 0,
                        sizeof (gsyncGetControlSyncParams));
                    gsyncGetControlSyncParams.displays = (1 << i);
                    //gsyncGetControlSyncParams.validateExternal = 1;
                    gsyncGetControlSyncParams.gpuId =
                        gsyncGpuTopologyParams.gpus[j].gpuId;
                    retVal = LwRmControl(m_hClient, hDevice,
                        LW30F1_CTRL_CMD_GSYNC_GET_CONTROL_SYNC,
                        &gsyncGetControlSyncParams,
                        sizeof (gsyncGetControlSyncParams));
                    rc = RmApiStatusToModsRC(retVal);
                    if(OK != rc)
                    {
                        Printf(Tee::PriHigh, "Get control sync failed\n");
                        // Free Devices
                        LwRmFree(m_hClient, hGpuDev, m_hDisplay);
                        LwRmFree(m_hClient, m_hClient, hGpuDev);
                        LwRmFree(m_hClient, m_hClient, hDevice);
                    }
                    CHECK_RC(rc);

                    PrintGsyncGetControlSync(gsyncGetControlSyncParams);
                }
            }

            //Free LW04_DISPLAY_COMMON  Device
            LwRmFree(m_hClient, hGpuDev, m_hDisplay);

            //Free GPU Device
            LwRmFree(m_hClient, m_hClient, hGpuDev);
        }

        //Free Device
        LwRmFree(m_hClient, m_hClient, hDevice);
        CHECK_RC(rc);
    }

    return rc;
}

//! \brief Print gsyncGetControlSyncParams
//!
//-----------------------------------------------------------------------------
void ClassGsyncTest::PrintGsyncGetControlSync(
    LW30F1_CTRL_GSYNC_GET_CONTROL_SYNC_PARAMS gsyncGetControlSyncParams
    )
{
    Printf(Tee::PriHigh, "gpuId            : 0x%x\n",
        (UINT32)gsyncGetControlSyncParams.gpuId);
    Printf(Tee::PriHigh, "master           : 0x%x\n",
        (UINT32)gsyncGetControlSyncParams.master);
    Printf(Tee::PriHigh, "displays         : 0x%x\n",
        (UINT32)gsyncGetControlSyncParams.displays);
    Printf(Tee::PriHigh, "validateExternal : 0x%x\n",
        (UINT32)gsyncGetControlSyncParams.validateExternal);
    Printf(Tee::PriHigh, "refresh          : 0x%x\n",
        (UINT32)gsyncGetControlSyncParams.refresh);
}

//! \brief Test for command LW30F1_CTRL_CMD_GSYNC_GET_CONTROL_INTERLACE_MODE
//! and LW30F1_CTRL_CMD_GSYNC_SET_CONTROL_INTERLACE_MODE
//!
//! \return OK if the test passed, specific RC if it failed
//-----------------------------------------------------------------------------
RC ClassGsyncTest::TestGsyncControlInterlaceMode()
{
    RC rc = OK;
    UINT32 retVal;
    LW30F1_ALLOC_PARAMETERS Lw30f1AllocParams;
    LW0000_CTRL_GSYNC_GET_ID_INFO_PARAMS gsyncIdInfoParams;
    LW30F1_CTRL_GSYNC_GET_CONTROL_INTERLACE_MODE_PARAMS
        getControlInterlaceModeParams;
    LW30F1_CTRL_GSYNC_SET_CONTROL_INTERLACE_MODE_PARAMS
        setControlInterlaceModeParams;
    LwRm::Handle hDevice = 0;
    UINT32 i;

    Printf(Tee::PriHigh, "Entering TestGsyncControlInterlaceMode()\n");

    for(i = 0; i < LW30F1_MAX_GSYNCS; i++)
    {
        if(LW0000_CTRL_GSYNC_ILWALID_ID == m_gsyncIdsTable.gsyncIds[i])
            break;

        Printf(Tee::PriHigh, "Testing GSync Device [Id:0x%x:]\n",
            (UINT32)m_gsyncIdsTable.gsyncIds[i]);

        // Get Gsync Instance
        memset(&gsyncIdInfoParams, 0, sizeof (gsyncIdInfoParams));
        gsyncIdInfoParams.gsyncId = m_gsyncIdsTable.gsyncIds[i];
        retVal = LwRmControl(m_hClient,
            m_hClient,
            LW0000_CTRL_CMD_GSYNC_GET_ID_INFO,
            &gsyncIdInfoParams,
            sizeof (gsyncIdInfoParams));
        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
            Printf(Tee::PriHigh,
                "Error in call LW0000_CTRL_CMD_GSYNC_GET_ID_INFO.\n");
        CHECK_RC(rc);

        // Allocate GSYNC Device Object
        memset(&Lw30f1AllocParams, 0, sizeof (Lw30f1AllocParams));
        hDevice = GetDeviceHandle();
        Lw30f1AllocParams.gsyncInstance = gsyncIdInfoParams.gsyncInstance;
        retVal = LwRmAlloc(m_hClient, m_hClient, hDevice,
            LW30_GSYNC,
            &Lw30f1AllocParams);
        CHECK_RC(RmApiStatusToModsRC(retVal));

        // Get Control Interlace Mode
        memset(&getControlInterlaceModeParams, 0,
            sizeof (getControlInterlaceModeParams));
        retVal = LwRmControl(m_hClient,
            hDevice,
            LW30F1_CTRL_CMD_GSYNC_GET_CONTROL_INTERLACE_MODE,
            &getControlInterlaceModeParams,
            sizeof (getControlInterlaceModeParams));
        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
        {
            Printf(Tee::PriHigh,
                "GET_CONTROL_INTERLACE_MODE failed.\n");
            // Free Device
            LwRmFree(m_hClient, m_hClient, hDevice);
        }
        CHECK_RC(rc);

        // Print Interlace Mode Status
        Printf(Tee::PriHigh, "Printing Interlace Mode Status :\n");
        Printf(Tee::PriHigh, "Enable : 0x%x\n",
            (UINT32)getControlInterlaceModeParams.enable);

        // Toggle Control Interlace Mode
        memset(&setControlInterlaceModeParams, 0,
            sizeof (setControlInterlaceModeParams));
        if(getControlInterlaceModeParams.enable)
            setControlInterlaceModeParams.enable = 0;
        else
            setControlInterlaceModeParams.enable = 1;
        retVal = LwRmControl(m_hClient,
            hDevice,
            LW30F1_CTRL_CMD_GSYNC_SET_CONTROL_INTERLACE_MODE,
            &setControlInterlaceModeParams,
            sizeof (setControlInterlaceModeParams));
        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
        {
            Printf(Tee::PriHigh,
                "SET_CONTROL_INTERLACE_MODE failed.\n");
            // Free Device
            LwRmFree(m_hClient, m_hClient, hDevice);
        }
        CHECK_RC(rc);

        // Reread Control Interlace Mode
        memset(&getControlInterlaceModeParams, 0,
            sizeof (getControlInterlaceModeParams));
        retVal = LwRmControl(m_hClient,
            hDevice,
            LW30F1_CTRL_CMD_GSYNC_GET_CONTROL_INTERLACE_MODE,
            &getControlInterlaceModeParams,
            sizeof (getControlInterlaceModeParams));
        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
        {
            Printf(Tee::PriHigh,
                "GET_CONTROL_INTERLACE_MODE failed.\n");
            // Free Device
            LwRmFree(m_hClient, m_hClient, hDevice);
        }
        CHECK_RC(rc);

        // Print Interlace Mode Status After Set
        Printf(Tee::PriHigh, "Printing Interlace Mode Status after Toggle:\n");
        Printf(Tee::PriHigh, "Enable : 0x%x\n",
            (UINT32)getControlInterlaceModeParams.enable);

        //Validate
        if(getControlInterlaceModeParams.enable !=
            setControlInterlaceModeParams.enable)
        {
            Printf(Tee::PriHigh, "Unexpected behaviour in INTERLACE_MODE.\n");
             rc = RC::SOFTWARE_ERROR;
        }

        //Free Device
        LwRmFree(m_hClient, m_hClient, hDevice);
        CHECK_RC(rc);
    }

    return rc;
}

//! \brief Test for command LW30F1_CTRL_CMD_GSYNC_SET_CONTROL_WATCHDOG
//!
//! \return OK if the test passed, specific RC if it failed
//-----------------------------------------------------------------------------
RC ClassGsyncTest::TestGsyncControlWatchDog()
{
    RC rc = OK;
    UINT32 retVal;
    LW30F1_ALLOC_PARAMETERS Lw30f1AllocParams;
    LW0000_CTRL_GSYNC_GET_ID_INFO_PARAMS gsyncIdInfoParams;
    LW30F1_CTRL_GSYNC_SET_CONTROL_WATCHDOG_PARAMS
        setControlWatchDogParams;
    LwRm::Handle hDevice = 0;
    UINT32 i;

    Printf(Tee::PriHigh, "Entering TestGsyncControlWatchDog()\n");

    for(i = 0; i < LW30F1_MAX_GSYNCS; i++)
    {
        if(LW0000_CTRL_GSYNC_ILWALID_ID == m_gsyncIdsTable.gsyncIds[i])
            break;

        Printf(Tee::PriHigh, "Testing GSync Device [Id:0x%x:]\n",
            (UINT32)m_gsyncIdsTable.gsyncIds[i]);

        // Get Gsync Instance
        memset(&gsyncIdInfoParams, 0, sizeof (gsyncIdInfoParams));
        gsyncIdInfoParams.gsyncId = m_gsyncIdsTable.gsyncIds[i];
        retVal = LwRmControl(m_hClient,
            m_hClient,
            LW0000_CTRL_CMD_GSYNC_GET_ID_INFO,
            &gsyncIdInfoParams,
            sizeof (gsyncIdInfoParams));
        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
            Printf(Tee::PriHigh,
                "Error in call LW0000_CTRL_CMD_GSYNC_GET_ID_INFO.\n");
        CHECK_RC(rc);

        // Allocate GSYNC Device Object
        memset(&Lw30f1AllocParams, 0, sizeof (Lw30f1AllocParams));
        hDevice = GetDeviceHandle();
        Lw30f1AllocParams.gsyncInstance = gsyncIdInfoParams.gsyncInstance;
        retVal = LwRmAlloc(m_hClient, m_hClient, hDevice,
            LW30_GSYNC,
            &Lw30f1AllocParams);
        CHECK_RC(RmApiStatusToModsRC(retVal));

        // Disable Control WatchDog Status
        memset(&setControlWatchDogParams, 0,
            sizeof (setControlWatchDogParams));
        retVal = LwRmControl(m_hClient,
            hDevice,
            LW30F1_CTRL_CMD_GSYNC_SET_CONTROL_WATCHDOG,
            &setControlWatchDogParams,
            sizeof (setControlWatchDogParams));
        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
        {
            Printf(Tee::PriHigh,
                "SET_CONTROL_WATCHDOG failed.\n");
            // Free Device
            LwRmFree(m_hClient, m_hClient, hDevice);
        }
        CHECK_RC(rc);

        // Print Control WatchDog Status
        Printf(Tee::PriHigh,
            "Printing Control WatchDog Status (after Disable):\n");
        Printf(Tee::PriHigh, "Enable : 0x%x\n",
            (UINT32)setControlWatchDogParams.enable);

        // TODO : Validate the change in WatchDog Status

        // Enable Control WatchDog Status
        memset(&setControlWatchDogParams, 0,
            sizeof (setControlWatchDogParams));
        setControlWatchDogParams.enable = 1;
        retVal = LwRmControl(m_hClient,
            hDevice,
            LW30F1_CTRL_CMD_GSYNC_SET_CONTROL_WATCHDOG,
            &setControlWatchDogParams,
            sizeof (setControlWatchDogParams));
        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
        {
            Printf(Tee::PriHigh,
                "SET_CONTROL_WATCHDOG failed.\n");
            // Free Device
            LwRmFree(m_hClient, m_hClient, hDevice);
        }
        CHECK_RC(rc);

        // Print Control WatchDog Status
        Printf(Tee::PriHigh,
            "Printing Control WatchDog Status (after Enable):\n");
        Printf(Tee::PriHigh, "Enable : 0x%x\n",
            (UINT32)setControlWatchDogParams.enable);

        // TODO : Validate the change in WatchDog Status

        //Free Device
        LwRmFree(m_hClient, m_hClient, hDevice);
        CHECK_RC(rc);
    }

    return rc;
}

//! \brief Test for command LW30F1_CTRL_CMD_GSYNC_GET_CONTROL_TESTING
//! and LW30F1_CTRL_CMD_GSYNC_SET_CONTROL_TESTING
//!
//! \return OK if the test passed, specific RC if it failed
//-----------------------------------------------------------------------------
RC ClassGsyncTest::TestGsyncControlTesting()
{
    RC rc = OK;
    UINT32 retVal;
    LW30F1_ALLOC_PARAMETERS Lw30f1AllocParams;
    LW0000_CTRL_GSYNC_GET_ID_INFO_PARAMS gsyncIdInfoParams;
    LW30F1_CTRL_GSYNC_GET_CONTROL_TESTING_PARAMS
        getControlTestingParams;
    LW30F1_CTRL_GSYNC_SET_CONTROL_TESTING_PARAMS
        setControlTestingParams;
    LwRm::Handle hDevice = 0;
    UINT32 i;

    Printf(Tee::PriHigh, "Entering TestGsyncControlTesting()\n");

    for(i = 0; i < LW30F1_MAX_GSYNCS; i++)
    {
        if(LW0000_CTRL_GSYNC_ILWALID_ID == m_gsyncIdsTable.gsyncIds[i])
            break;

        Printf(Tee::PriHigh, "Testing GSync Device [Id:0x%x:]\n",
            (UINT32)m_gsyncIdsTable.gsyncIds[i]);

        // Get Gsync Instance
        memset(&gsyncIdInfoParams, 0, sizeof (gsyncIdInfoParams));
        gsyncIdInfoParams.gsyncId = m_gsyncIdsTable.gsyncIds[i];
        retVal = LwRmControl(m_hClient,
            m_hClient,
            LW0000_CTRL_CMD_GSYNC_GET_ID_INFO,
            &gsyncIdInfoParams,
            sizeof (gsyncIdInfoParams));
        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
            Printf(Tee::PriHigh,
                "Error in call LW0000_CTRL_CMD_GSYNC_GET_ID_INFO.\n");
        CHECK_RC(rc);

        // Allocate GSYNC Device Object
        memset(&Lw30f1AllocParams, 0, sizeof (Lw30f1AllocParams));
        hDevice = GetDeviceHandle();
        Lw30f1AllocParams.gsyncInstance = gsyncIdInfoParams.gsyncInstance;
        retVal = LwRmAlloc(m_hClient, m_hClient, hDevice,
            LW30_GSYNC,
            &Lw30f1AllocParams);
        CHECK_RC(RmApiStatusToModsRC(retVal));

        // Get Control Testing Status
        memset(&getControlTestingParams, 0,
            sizeof (getControlTestingParams));
        retVal = LwRmControl(m_hClient,
            hDevice,
            LW30F1_CTRL_CMD_GSYNC_GET_CONTROL_TESTING,
            &getControlTestingParams,
            sizeof (getControlTestingParams));
        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
        {
            Printf(Tee::PriHigh,
                "GET_CONTROL_TESTING failed.\n");
            // Free Device
            LwRmFree(m_hClient, m_hClient, hDevice);
        }
        CHECK_RC(rc);

        // Print Control Testing Status
        Printf(Tee::PriHigh, "Printing Control Testing Status :\n");
        Printf(Tee::PriHigh, "bEmitTestSignal : 0x%x\n",
            (UINT32)getControlTestingParams.bEmitTestSignal);

        // Toggle Control Testing Mode
        memset(&setControlTestingParams, 0,
            sizeof (setControlTestingParams));
        if(getControlTestingParams.bEmitTestSignal)
            setControlTestingParams.bEmitTestSignal = 0;
        else
            setControlTestingParams.bEmitTestSignal = 1;
        retVal = LwRmControl(m_hClient,
            hDevice,
            LW30F1_CTRL_CMD_GSYNC_SET_CONTROL_TESTING,
            &setControlTestingParams,
            sizeof (setControlTestingParams));
        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
        {
            Printf(Tee::PriHigh,
                "SET_CONTROL_TESTING failed."
                " Ensure that the displays are synchronized.\n");
            // Free Device
            LwRmFree(m_hClient, m_hClient, hDevice);
        }
        CHECK_RC(rc);

        // Reread Control Testing Status
        memset(&getControlTestingParams, 0,
            sizeof (getControlTestingParams));
        retVal = LwRmControl(m_hClient,
            hDevice,
            LW30F1_CTRL_CMD_GSYNC_GET_CONTROL_TESTING,
            &getControlTestingParams,
            sizeof (getControlTestingParams));
        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
        {
            Printf(Tee::PriHigh,
                "GET_CONTROL_TESTING failed.\n");
            // Free Device
            LwRmFree(m_hClient, m_hClient, hDevice);
        }
        CHECK_RC(rc);

        // Print Control Testing Status After Set
        Printf(Tee::PriHigh, "Printing Control Testing Status after Toggle:\n");
        Printf(Tee::PriHigh, "bEmitTestSignal : 0x%x\n",
            (UINT32)getControlTestingParams.bEmitTestSignal);

        //Validate
        if(getControlTestingParams.bEmitTestSignal !=
            setControlTestingParams.bEmitTestSignal)
        {
            Printf(Tee::PriHigh, "Unexpected behaviour in CONTROL_TESTING.\n");
             rc = RC::SOFTWARE_ERROR;
        }

        //Free Device
        LwRmFree(m_hClient, m_hClient, hDevice);
        CHECK_RC(rc);
    }

    return rc;
}

//! \brief Test for command LW30F1_CTRL_CMD_GSYNC_GET_STATUS
//!
//! \return OK if the test passed, specific RC if it failed
//-----------------------------------------------------------------------------
RC ClassGsyncTest::TestGsyncGetStatus()
{
    RC rc = OK;
    UINT32 retVal;
    LW30F1_ALLOC_PARAMETERS Lw30f1AllocParams;
    LW0000_CTRL_GSYNC_GET_ID_INFO_PARAMS gsyncIdInfoParams;
    LW30F1_CTRL_GSYNC_GET_STATUS_PARAMS
        gsyncGetStatusParams;
    LwRm::Handle hDevice = 0;
    UINT32 i;

    Printf(Tee::PriHigh, "Entering TestGsyncGetStatus()\n");

    for(i = 0; i < LW30F1_MAX_GSYNCS; i++)
    {
        if(LW0000_CTRL_GSYNC_ILWALID_ID == m_gsyncIdsTable.gsyncIds[i])
            break;

        Printf(Tee::PriHigh, "Testing GSync Device [Id:0x%x:]\n",
            (UINT32)m_gsyncIdsTable.gsyncIds[i]);

        // Get Gsync Instance
        memset(&gsyncIdInfoParams, 0, sizeof (gsyncIdInfoParams));
        gsyncIdInfoParams.gsyncId = m_gsyncIdsTable.gsyncIds[i];
        retVal = LwRmControl(m_hClient,
            m_hClient,
            LW0000_CTRL_CMD_GSYNC_GET_ID_INFO,
            &gsyncIdInfoParams,
            sizeof (gsyncIdInfoParams));
        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
            Printf(Tee::PriHigh,
                "Error in call LW0000_CTRL_CMD_GSYNC_GET_ID_INFO.\n");
        CHECK_RC(rc);

        // Allocate GSYNC Device Object
        memset(&Lw30f1AllocParams, 0, sizeof (Lw30f1AllocParams));
        hDevice = GetDeviceHandle();
        Lw30f1AllocParams.gsyncInstance = gsyncIdInfoParams.gsyncInstance;
        retVal = LwRmAlloc(m_hClient, m_hClient, hDevice,
            LW30_GSYNC,
            &Lw30f1AllocParams);
        CHECK_RC(RmApiStatusToModsRC(retVal));

        // Get Gsync Status
        memset(&gsyncGetStatusParams, 0,
            sizeof (gsyncGetStatusParams));
        gsyncGetStatusParams.which
            |= LW30F1_CTRL_GSYNC_GET_STATUS_SYNC_POLARITY;
        gsyncGetStatusParams.which
            |= LW30F1_CTRL_GSYNC_GET_STATUS_LEADING_EDGE;
        gsyncGetStatusParams.which
            |= LW30F1_CTRL_GSYNC_GET_STATUS_FALLING_EDGE;
        gsyncGetStatusParams.which |= LW30F1_CTRL_GSYNC_GET_STATUS_SYNC_DELAY;
        gsyncGetStatusParams.which |= LW30F1_CTRL_GSYNC_GET_STATUS_REFRESH;
        gsyncGetStatusParams.which
            |= LW30F1_CTRL_GSYNC_GET_STATUS_HOUSE_SYNC_INCOMING;
        gsyncGetStatusParams.which
            |= LW30F1_CTRL_GSYNC_GET_STATUS_SYNC_INTERVAL;
        gsyncGetStatusParams.which |= LW30F1_CTRL_GSYNC_GET_STATUS_SYNC_READY;
        gsyncGetStatusParams.which |= LW30F1_CTRL_GSYNC_GET_STATUS_SWAP_READY;
        gsyncGetStatusParams.which |= LW30F1_CTRL_GSYNC_GET_STATUS_TIMING;
        gsyncGetStatusParams.which |= LW30F1_CTRL_GSYNC_GET_STATUS_STEREO_SYNC;
        gsyncGetStatusParams.which |= LW30F1_CTRL_GSYNC_GET_STATUS_HOUSE_SYNC;
        gsyncGetStatusParams.which |= LW30F1_CTRL_GSYNC_GET_STATUS_PORT_INPUT;
        gsyncGetStatusParams.which |= LW30F1_CTRL_GSYNC_GET_STATUS_PORT0_INPUT;
        gsyncGetStatusParams.which |= LW30F1_CTRL_GSYNC_GET_STATUS_PORT1_INPUT;
        gsyncGetStatusParams.which
            |= LW30F1_CTRL_GSYNC_GET_STATUS_PORT_ETHERNET;
        gsyncGetStatusParams.which
            |= LW30F1_CTRL_GSYNC_GET_STATUS_PORT0_ETHERNET;
        gsyncGetStatusParams.which
            |= LW30F1_CTRL_GSYNC_GET_STATUS_PORT1_ETHERNET;
        gsyncGetStatusParams.which
            |= LW30F1_CTRL_GSYNC_GET_STATUS_UNIVERSAL_FRAME_COUNT;
        retVal = LwRmControl(m_hClient,
            hDevice,
            LW30F1_CTRL_CMD_GSYNC_GET_STATUS,
            &gsyncGetStatusParams,
            sizeof (gsyncGetStatusParams));
        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
        {
            Printf(Tee::PriHigh,
                "GSYNC_GET_STATUS failed.\n");
            // Free Device
            LwRmFree(m_hClient, m_hClient, hDevice);
        }
        CHECK_RC(rc);

        Printf(Tee::PriHigh, "Printing Gsync Status Paremeters :\n");
        Printf(Tee::PriHigh, "bLeadingEdge        : 0x%x\n",
            (UINT32)gsyncGetStatusParams.bLeadingEdge);
        Printf(Tee::PriHigh, "bFallingEdge        : 0x%x\n",
            (UINT32)gsyncGetStatusParams.bFallingEdge);
        Printf(Tee::PriHigh, "syncDelay           : 0x%x\n",
            (UINT32)gsyncGetStatusParams.syncDelay);
        Printf(Tee::PriHigh, "refresh             : 0x%x\n",
            (UINT32)gsyncGetStatusParams.refresh);
        Printf(Tee::PriHigh, "houseSyncIncoming   : 0x%x\n",
            (UINT32)gsyncGetStatusParams.houseSyncIncoming);
        Printf(Tee::PriHigh, "syncInterval        : 0x%x\n",
            (UINT32)gsyncGetStatusParams.syncInterval);
        Printf(Tee::PriHigh, "bSyncReady          : 0x%x\n",
            (UINT32)gsyncGetStatusParams.bSyncReady);
        Printf(Tee::PriHigh, "bSwapReady          : 0x%x\n",
            (UINT32)gsyncGetStatusParams.bSwapReady);
        Printf(Tee::PriHigh, "bHouseSync          : 0x%x\n",
            (UINT32)gsyncGetStatusParams.bHouseSync);
        Printf(Tee::PriHigh, "bPort0Input         : 0x%x\n",
            (UINT32)gsyncGetStatusParams.bPort0Input);
        Printf(Tee::PriHigh, "bPort1Input         : 0x%x\n",
            (UINT32)gsyncGetStatusParams.bPort1Input);
        Printf(Tee::PriHigh, "bPort0Ethernet      : 0x%x\n",
            (UINT32)gsyncGetStatusParams.bPort0Ethernet);
        Printf(Tee::PriHigh, "bPort1Ethernet      : 0x%x\n",
            (UINT32)gsyncGetStatusParams.bPort1Ethernet);
        Printf(Tee::PriHigh, "universalFrameCount : 0x%x\n",
            (UINT32)gsyncGetStatusParams.universalFrameCount);

        //Free Device
        LwRmFree(m_hClient, m_hClient, hDevice);
        CHECK_RC(rc);
    }

    return rc;
}

//! \brief Test for command LW30F1_CTRL_CMD_GSYNC_GET_CONTROL_PARAMS and
//!  LW30F1_CTRL_CMD_GSYNC_SET_CONTROL_PARAMS
//!
//! \return OK if the test passed, specific RC if it failed
//-----------------------------------------------------------------------------
RC ClassGsyncTest::TestGsyncControlParams()
{
    RC            rc = OK;
    UINT32        retVal, i;
    LwRm::Handle  hDevice = 0;

    LW30F1_ALLOC_PARAMETERS                      Lw30f1AllocParams;
    LW0000_CTRL_GSYNC_GET_ID_INFO_PARAMS         gsyncIdInfoParams;
    LW30F1_CTRL_GSYNC_GET_CONTROL_PARAMS_PARAMS  getControlParams;
    LW30F1_CTRL_GSYNC_SET_CONTROL_PARAMS_PARAMS  setControlParams;

    Printf(Tee::PriHigh, "Entering TestGsyncControlParams()\n");
    for(i = 0; i < LW30F1_MAX_GSYNCS; i++)
    {
        if(LW0000_CTRL_GSYNC_ILWALID_ID == m_gsyncIdsTable.gsyncIds[i])
            break;

        Printf(Tee::PriHigh, "Testing GSync Device [Id:0x%x:]\n",
               (UINT32)m_gsyncIdsTable.gsyncIds[i]);

        // Get Gsync Instance
        memset(&gsyncIdInfoParams, 0, sizeof (gsyncIdInfoParams));
        gsyncIdInfoParams.gsyncId = m_gsyncIdsTable.gsyncIds[i];

        retVal = LwRmControl(m_hClient,
                             m_hClient,
                             LW0000_CTRL_CMD_GSYNC_GET_ID_INFO,
                             &gsyncIdInfoParams,
                             sizeof (gsyncIdInfoParams));
        rc = RmApiStatusToModsRC(retVal);
        if(OK != rc)
        {
            Printf(Tee::PriHigh,
                   "Error related to GET INFO cmd.\n");
            CHECK_RC(rc);
        }

        // Allocate GSYNC Device Object
        memset(&Lw30f1AllocParams, 0, sizeof (Lw30f1AllocParams));
        hDevice = GetDeviceHandle();
        Lw30f1AllocParams.gsyncInstance = gsyncIdInfoParams.gsyncInstance;
        retVal = LwRmAlloc(m_hClient,
                           m_hClient,
                           hDevice,
                           LW30_GSYNC,
                           &Lw30f1AllocParams);
        CHECK_RC(RmApiStatusToModsRC(retVal));

        // Set Gsync Control Params
        memset(&setControlParams, 0, sizeof (setControlParams));

        setControlParams.which |= LW30F1_CTRL_GSYNC_SET_CONTROL_SYNC_POLARITY;
        setControlParams.which |= LW30F1_CTRL_GSYNC_SET_CONTROL_VIDEO_MODE;
        setControlParams.which |= LW30F1_CTRL_GSYNC_SET_CONTROL_NSYNC;
        setControlParams.which |= LW30F1_CTRL_GSYNC_SET_CONTROL_SYNC_SKEW;
        setControlParams.which |= LW30F1_CTRL_GSYNC_SET_CONTROL_SYNC_START_DELAY;
        setControlParams.which |= LW30F1_CTRL_GSYNC_SET_CONTROL_SYNC_USE_HOUSE;

        setControlParams.syncPolarity =
            LW30F1_CTRL_GSYNC_SET_CONTROL_SYNC_POLARITY_RISING_EDGE;
        setControlParams.syncVideoMode =
            LW30F1_CTRL_GSYNC_SET_CONTROL_VIDEO_MODE_NTSCPALSECAM;
        setControlParams.nSync          = NSYNC;
        setControlParams.syncSkew       = SYNC_SKEW;
        setControlParams.syncStartDelay = SYNC_START_DELAY;
        setControlParams.useHouseSync   = 1;

        // Set control params
        retVal = LwRmControl(m_hClient,
                             hDevice,
                             LW30F1_CTRL_CMD_GSYNC_SET_CONTROL_PARAMS,
                             &setControlParams,
                             sizeof (setControlParams));
        rc = RmApiStatusToModsRC(retVal);

        if(OK != rc)
        {
            Printf(Tee::PriHigh,
                "GSYNC_SET_CONTROL_PARAMS failed.\n");
            // Free Device
            LwRmFree(m_hClient, m_hClient, hDevice);
        }
        CHECK_RC(rc);

        // Get Gsync Control Params
        memset(&getControlParams, 0, sizeof (getControlParams));

        getControlParams.which |= LW30F1_CTRL_GSYNC_GET_CONTROL_SYNC_POLARITY;
        getControlParams.which |= LW30F1_CTRL_GSYNC_GET_CONTROL_VIDEO_MODE;
        getControlParams.which |= LW30F1_CTRL_GSYNC_GET_CONTROL_NSYNC;
        getControlParams.which |= LW30F1_CTRL_GSYNC_GET_CONTROL_SYNC_SKEW;
        getControlParams.which |= LW30F1_CTRL_GSYNC_GET_CONTROL_SYNC_START_DELAY;
        getControlParams.which |= LW30F1_CTRL_GSYNC_SET_CONTROL_SYNC_USE_HOUSE;

        // Get control params
        retVal = LwRmControl(m_hClient,
                             hDevice,
                             LW30F1_CTRL_CMD_GSYNC_GET_CONTROL_PARAMS,
                             &getControlParams,
                             sizeof (getControlParams));
        rc = RmApiStatusToModsRC(retVal);

        if(OK != rc)
        {
            Printf(Tee::PriHigh,
                "GSYNC_GET_CONTROL_PARAMS failed.\n");
            // Free Device
            LwRmFree(m_hClient, m_hClient, hDevice);
        }
        CHECK_RC(rc);

        Printf(Tee::PriHigh, "Printing Gsync Control Parameters :\n");
        Printf(Tee::PriHigh, "syncPolarity    : 0x%x\n",
               (UINT32)getControlParams.syncPolarity);
        Printf(Tee::PriHigh, "syncVideoMode   : 0x%x\n",
               (UINT32)getControlParams.syncVideoMode);
        Printf(Tee::PriHigh, "nSync           : 0x%x\n",
               (UINT32)getControlParams.nSync);
        Printf(Tee::PriHigh, "syncSkew        : 0x%x\n",
               (UINT32)getControlParams.syncSkew);
        Printf(Tee::PriHigh, "syncStartDelay  : 0x%x\n",
               (UINT32)getControlParams.syncStartDelay);
        Printf(Tee::PriHigh, "useHouseSync    : 0x%x\n",
               (UINT32)getControlParams.useHouseSync);

        if (getControlParams.syncPolarity != setControlParams.syncPolarity)
        {
            Printf(Tee::PriHigh,
                   "syncPolarity is different from the set value.\n");
             rc = RC::SOFTWARE_ERROR;
        }
        if (getControlParams.syncVideoMode != setControlParams.syncVideoMode)
        {
            Printf(Tee::PriHigh,
                   "syncPolarity is different from the set value.\n");
             rc = RC::SOFTWARE_ERROR;
        }
        if (NSYNC != getControlParams.nSync)
        {
            Printf(Tee::PriHigh,
                   "syncPolarity is different from the set value.\n");
             rc = RC::SOFTWARE_ERROR;
        }
        if (SYNC_SKEW != getControlParams.syncSkew)
        {
            Printf(Tee::PriHigh,
                   "syncSkew is different from the set value.\n");
             rc = RC::SOFTWARE_ERROR;
        }
        if (SYNC_START_DELAY != getControlParams.syncStartDelay)
        {
            Printf(Tee::PriHigh,
                   "syncStartDelay is different from the set value.\n");
             rc = RC::SOFTWARE_ERROR;
        }
        if (getControlParams.useHouseSync != setControlParams.useHouseSync)
        {
            Printf(Tee::PriHigh,
                   "useHouseSync is different from the set value.\n");
             rc = RC::SOFTWARE_ERROR;
        }

        //Free Device
        LwRmFree(m_hClient, m_hClient, hDevice);
        CHECK_RC(rc);
    }
    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ ClassGsyncTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(ClassGsyncTest, RmTest,
                 "GsyncTest RM test.");
