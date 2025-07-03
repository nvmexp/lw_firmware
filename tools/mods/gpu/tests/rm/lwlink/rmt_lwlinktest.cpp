/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2016,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_lwlinksysmem.cpp
//! Basic sysmem access test for LWLink
//!
#include <map>
#include <set>
#include <string> // Only use <> for built-in C++ stuff
#include <vector>

#include "rmt_lwlinktests.h"
#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "gpu/include/gpumgr.h"
#include "core/include/mgrmgr.h"

#include "core/include/memcheck.h"

class LWLINKTest : public RmTest
{

public:
    LWLINKTest();
    virtual ~LWLINKTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    GpuDevMgr *m_pGpuDevMgr;
    UINT32    m_NumDevices;

    GpuSubdevice  *m_pSubdevice;

    GpuDevice     *m_pGpuDevFirst;
    GpuDevice     *m_pGpuDevSecond;

};

//! \brief Constructor of LWLINKTest
//------------------------------------------------------------------------------
LWLINKTest::LWLINKTest() :
    m_pGpuDevMgr(nullptr),
    m_NumDevices(0),
    m_pSubdevice(nullptr),
    m_pGpuDevFirst(nullptr),
    m_pGpuDevSecond(nullptr)
{
    SetName("LWLINKTest");
}

//! \brief Destructor of LWLINKTest
//!
//!     Destructor of LWLINKTest
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
LWLINKTest::~LWLINKTest()
{

}

//! \brief IsTestSupported Function
//!
//!     1. Check whether the chip family supported.
//!
//! \return RC::OK if the test can be run in the current environment,
//!         skip reason otherwise
//------------------------------------------------------------------------------
string LWLINKTest::IsTestSupported()
{
    if (!(GetBoundGpuSubdevice()->DeviceId() >= Gpu::GP100))
        return "Test only supported on GP100 or later chips";
    return RUN_RMTEST_TRUE;
}

//! \brief Setup Function
//!
//! \return Should always return RM::OK
//------------------------------------------------------------------------------
RC LWLINKTest::Setup()
{
    RC      rc = OK;
    LwRmPtr pLwRm;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    m_pGpuDevMgr = (GpuDevMgr*)DevMgrMgr::d_GraphDevMgr;

    m_NumDevices = 0;

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
        m_NumDevices++;
    }

    if (!m_pSubdevice)
        m_pSubdevice = GetBoundGpuSubdevice();

    // 1. Consider 2 GpuSubdevices say pGpuDevFirst and pGpuDevSecond.
    m_pGpuDevFirst = m_pGpuDevMgr->GetFirstGpuDevice();

    if (m_NumDevices == 1 )
    {
       // Loopback setting;
        m_pGpuDevSecond = m_pGpuDevFirst;
    }
    else
    {
       m_pGpuDevSecond = m_pGpuDevMgr->GetNextGpuDevice(m_pGpuDevFirst);
    }

    return rc;
}

//! \brief Run Function
//!
//! Lwrrently only checks for LWLink caps
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------------
RC LWLINKTest::Run()
{
    int i;
    UINT32 j;
    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    LW2080_CTRL_CMD_LWLINK_GET_LWLINK_CAPS_PARAMS lwLinkCaps = {0};
    LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS lwLinkStatus = {0};
    LW2080_CTRL_LWLINK_GET_ERR_INFO_PARAMS lwlinkErr = {0};

    CHECK_RC(pLwRm->Control(hSubdevice,
                    LW2080_CTRL_CMD_LWLINK_GET_LWLINK_CAPS,
                    &lwLinkCaps, sizeof(lwLinkCaps)));

    Printf(Tee::PriHigh, "LWLink Capabilities:\n");
    if (!LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *)&lwLinkCaps.capsTbl), LW2080_CTRL_LWLINK_CAPS_SUPPORTED))
    {
        Printf(Tee::PriError, "    LWLink is not supported on this HW.\n");
        rc = RC::UNSUPPORTED_HARDWARE_FEATURE;
        return rc;
    }
    else
    {
        Printf(Tee::PriHigh, "    LWLink is supported on this HW.\n");
    }

    Printf(Tee::PriHigh, "    Enabled Link Mask = 0x%x\n", lwLinkCaps.enabledLinkMask);

    Printf(Tee::PriHigh, "    Sysmem Access : %s\n",
           (LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *)&lwLinkCaps.capsTbl), LW2080_CTRL_LWLINK_CAPS_SYSMEM_ACCESS) ? "Yes" : "No"));
    Printf(Tee::PriHigh, "    Sysmem Atomics : %s\n",
           (LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *)&lwLinkCaps.capsTbl), LW2080_CTRL_LWLINK_CAPS_SYSMEM_ATOMICS) ? "Yes" : "No"));
    Printf(Tee::PriHigh, "    P2P Supported : %s\n",
           (LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *)&lwLinkCaps.capsTbl), LW2080_CTRL_LWLINK_CAPS_P2P_SUPPORTED) ? "Yes" : "No"));
    Printf(Tee::PriHigh, "    P2P Atomics : %s\n",
           (LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *)&lwLinkCaps.capsTbl), LW2080_CTRL_LWLINK_CAPS_P2P_ATOMICS) ? "Yes" : "No"));
    Printf(Tee::PriHigh, "    PEX Tunneling : %s\n",
           (LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *)&lwLinkCaps.capsTbl), LW2080_CTRL_LWLINK_CAPS_PEX_TUNNELING) ? "Yes" : "No"));
    Printf(Tee::PriHigh, "    SLI Bridge : %s\n",
           (LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *)&lwLinkCaps.capsTbl), LW2080_CTRL_LWLINK_CAPS_SLI_BRIDGE) ? "Yes" : "No"));
    Printf(Tee::PriHigh, "    Power State L0 : %s\n",
           (LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *)&lwLinkCaps.capsTbl), LW2080_CTRL_LWLINK_CAPS_POWER_STATE_L0) ? "Yes" : "No"));
    Printf(Tee::PriHigh, "    Power State L1 : %s\n",
           (LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *)&lwLinkCaps.capsTbl), LW2080_CTRL_LWLINK_CAPS_POWER_STATE_L1) ? "Yes" : "No"));
    Printf(Tee::PriHigh, "    Power State L2 : %s\n",
           (LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *)&lwLinkCaps.capsTbl), LW2080_CTRL_LWLINK_CAPS_POWER_STATE_L2) ? "Yes" : "No"));
    Printf(Tee::PriHigh, "    Power State L3 : %s\n",
           (LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *)&lwLinkCaps.capsTbl), LW2080_CTRL_LWLINK_CAPS_POWER_STATE_L3) ? "Yes" : "No"));

    CHECK_RC(pLwRm->Control(hSubdevice,
                    LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS,
                    &lwLinkStatus, sizeof(lwLinkStatus)));

    Printf(Tee::PriHigh, "LWLink Status:\n");
    FOR_EACH_INDEX_IN_MASK(32, i, lwLinkCaps.enabledLinkMask)
    {
        Printf(Tee::PriHigh, "  Link %d :", i);
        if(!LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *)&lwLinkStatus.linkInfo[i].capsTbl),
                                       LW2080_CTRL_LWLINK_CAPS_VALID))
        {
            Printf(Tee::PriHigh, " Disabled\n");
            continue;
        }

        Printf(Tee::PriHigh, " \n");
        Printf(Tee::PriHigh, "    Sysmem Access : %s\n",
           (LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *)&lwLinkStatus.linkInfo[i].capsTbl),
                                       LW2080_CTRL_LWLINK_CAPS_SYSMEM_ACCESS) ? "Yes" : "No"));
        Printf(Tee::PriHigh, "    P2P Supported : %s\n",
           (LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *)&lwLinkStatus.linkInfo[i].capsTbl),
                                       LW2080_CTRL_LWLINK_CAPS_P2P_SUPPORTED) ? "Yes" : "No"));
        Printf(Tee::PriHigh, "    P2P Atomics : %s\n",
           (LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *)&lwLinkStatus.linkInfo[i].capsTbl),
                                       LW2080_CTRL_LWLINK_CAPS_P2P_ATOMICS) ? "Yes" : "No"));
        Printf(Tee::PriHigh, "    PEX Tunneling : %s\n",
           (LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *)&lwLinkStatus.linkInfo[i].capsTbl),
                                       LW2080_CTRL_LWLINK_CAPS_PEX_TUNNELING) ? "Yes" : "No"));
        Printf(Tee::PriHigh, "    SLI Bridge : %s\n",
           (LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *)&lwLinkStatus.linkInfo[i].capsTbl),
                                       LW2080_CTRL_LWLINK_CAPS_SLI_BRIDGE) ? "Yes" : "No"));
        Printf(Tee::PriHigh, "    Power State L0 : %s\n",
           (LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *)&lwLinkStatus.linkInfo[i].capsTbl),
                                       LW2080_CTRL_LWLINK_CAPS_POWER_STATE_L0) ? "Yes" : "No"));
        Printf(Tee::PriHigh, "    Power State L1 : %s\n",
           (LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *)&lwLinkStatus.linkInfo[i].capsTbl),
                                       LW2080_CTRL_LWLINK_CAPS_POWER_STATE_L1) ? "Yes" : "No"));
        Printf(Tee::PriHigh, "    Power State L2 : %s\n",
           (LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *)&lwLinkStatus.linkInfo[i].capsTbl),
                                       LW2080_CTRL_LWLINK_CAPS_POWER_STATE_L2) ? "Yes" : "No"));
        Printf(Tee::PriHigh, "    Power State L3 : %s\n",
           (LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *)&lwLinkStatus.linkInfo[i].capsTbl),
                                       LW2080_CTRL_LWLINK_CAPS_POWER_STATE_L3) ? "Yes" : "No"));

        Printf(Tee::PriHigh, "    PHY Type : %s\n", phyTypeNames[lwLinkStatus.linkInfo[i].phyType]);

        if (lwLinkStatus.linkInfo[i].linkState >= LWLINK_MAX_NUM_LINK_STATES)
        {
            Printf(Tee::PriHigh, "    Invalid Link State.\n");
        }
        else
        {
            Printf(Tee::PriHigh, "    Link State : %s\n", linkStateNames[lwLinkStatus.linkInfo[i].linkState]);
        }
        Printf(Tee::PriHigh, " \n");

        Printf(Tee::PriHigh, "    LWLINK Version : %d\n", lwLinkStatus.linkInfo[i].lwlinkVersion);
        Printf(Tee::PriHigh, "    NCI Version : %d\n", lwLinkStatus.linkInfo[i].nciVersion);
        Printf(Tee::PriHigh, "    PHY Version : %d\n", lwLinkStatus.linkInfo[i].phyVersion);

        if (lwLinkStatus.linkInfo[i].connected)
        {

            Printf(Tee::PriHigh, "    Clock Information\n");

            Printf(Tee::PriHigh, "      Link clock: %d MHz\n",
                                 lwLinkStatus.linkInfo[i].lwlinkLinkClockMhz);
            switch (lwLinkStatus.linkInfo[i].lwlinkRefClkType)
            {
                case LW2080_CTRL_LWLINK_REFCLK_TYPE_ILWALID:
                    Printf(Tee::PriHigh, "      Refclk: Invalid\n");
                    break;
                case LW2080_CTRL_LWLINK_REFCLK_TYPE_LWHS:
                    Printf(Tee::PriHigh, "      Refclk: LWHS @ %d MHz\n", lwLinkStatus.linkInfo[i].lwlinkRefClkSpeedMhz);
                    break;
                case LW2080_CTRL_LWLINK_REFCLK_TYPE_PEX:
                    Printf(Tee::PriHigh, "      Refclk: PEX @ %d MHz\n", lwLinkStatus.linkInfo[i].lwlinkRefClkSpeedMhz);
                    break;
                default:
                    break;
            }

            Printf(Tee::PriHigh, "      Common Clock: %d MHz\n",
                                 lwLinkStatus.linkInfo[i].lwlinkLinkClockMhz);

            Printf(Tee::PriHigh, "    Remote End Information\n");

            switch (lwLinkStatus.linkInfo[i].loopProperty)
            {
                case LW2080_CTRL_LWLINK_STATUS_LOOP_PROPERTY_NONE:
                    Printf(Tee::PriHigh, "      Link Property: No loop\n");
                    break;
                case LW2080_CTRL_LWLINK_STATUS_LOOP_PROPERTY_LOOPBACK:
                    Printf(Tee::PriHigh, "      Link Property: Loopback link\n");
                    break;
                case LW2080_CTRL_LWLINK_STATUS_LOOP_PROPERTY_LOOPOUT:
                    Printf(Tee::PriHigh, "      Link Property: Loopout link\n");
                    break;
                default:
                    break;
            }

            Printf(Tee::PriHigh, "      Remote Link Number: %d\n",
                                 lwLinkStatus.linkInfo[i].remoteDeviceLinkNumber);
            Printf(Tee::PriHigh, "      Remote Device ID Flags: %d\n",
                                 lwLinkStatus.linkInfo[i].remoteDeviceInfo.deviceIdFlags);
            Printf(Tee::PriHigh, "      Remote Device Domain: %d\n",
                                 lwLinkStatus.linkInfo[i].remoteDeviceInfo.domain);
            Printf(Tee::PriHigh, "      Remote Device Bus: %d\n",
                                 lwLinkStatus.linkInfo[i].remoteDeviceInfo.bus);
            Printf(Tee::PriHigh, "      Remote Device Device: %d\n",
                                 lwLinkStatus.linkInfo[i].remoteDeviceInfo.device);
            Printf(Tee::PriHigh, "      Remote Device Function: %d\n",
                                 lwLinkStatus.linkInfo[i].remoteDeviceInfo.function);
            Printf(Tee::PriHigh, "      Remote Device PCI Device ID: %d\n",
                                 lwLinkStatus.linkInfo[i].remoteDeviceInfo.pciDeviceId);

            switch (lwLinkStatus.linkInfo[i].remoteDeviceInfo.deviceType)
            {
                case LW2080_CTRL_LWLINK_DEVICE_INFO_DEVICE_TYPE_EBRIDGE:
                    Printf(Tee::PriHigh, "      Remote Device Type: EBRIDGE\n");
                    break;
                case LW2080_CTRL_LWLINK_DEVICE_INFO_DEVICE_TYPE_NPU:
                    Printf(Tee::PriHigh, "      Remote Device Type: NPU\n");
                    break;
                case LW2080_CTRL_LWLINK_DEVICE_INFO_DEVICE_TYPE_GPU:
                    Printf(Tee::PriHigh, "      Remote Device Type: GPU\n");
                    break;
                case LW2080_CTRL_LWLINK_DEVICE_INFO_DEVICE_TYPE_NONE:
                    Printf(Tee::PriHigh, "      Remote Device Type: Unidentified device\n");
                    break;
                default:
                    break;
            }

            Printf(Tee::PriHigh, "      Remote Device UUID:");
            for (j = 0; j < 16; j++)
            {
                Printf(Tee::PriHigh, " 0x%x", lwLinkStatus.linkInfo[i].remoteDeviceInfo.deviceUUID[j]);
            }
            Printf(Tee::PriHigh, "\n");
        }
        else
        {
            Printf(Tee::PriHigh, "    No device on remote end\n");
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    CHECK_RC(pLwRm->Control(hSubdevice,
                    LW2080_CTRL_CMD_LWLINK_GET_ERR_INFO,
                    &lwlinkErr, sizeof(lwlinkErr)));

    Printf(Tee::PriHigh, " \n");
    Printf(Tee::PriHigh, "LWLINK Error Information \n");
    Printf(Tee::PriHigh, " \n");
    Printf(Tee::PriHigh, "Enabled Link Mask = 0x%x\n", lwlinkErr.linkMask);
    Printf(Tee::PriHigh, " \n");

    FOR_EACH_INDEX_IN_MASK(32, i, lwlinkErr.linkMask)
    {
        Printf(Tee::PriHigh, "  Link %d :\n", i);

        Printf(Tee::PriHigh, " \n");
        Printf(Tee::PriHigh, "    DL Speed Status Tx: %d\n", lwlinkErr.linkErrInfo[i].DLSpeedStatusTx);
        Printf(Tee::PriHigh, "    DL Speed Status Rx: %d\n", lwlinkErr.linkErrInfo[i].DLSpeedStatusRx);
        Printf(Tee::PriHigh, "    Excess Error Rate DL : %s\n", lwlinkErr.linkErrInfo[i].bExcessErrorDL ? "True" : "False");
        Printf(Tee::PriHigh, " \n");

        for(j = 0; j < LW2080_CTRL_LWLINK_TL_INTEN_IDX_MAX; j++)
        {
            Printf(Tee::PriHigh, "    Interrupt enabled bit for error %u : %s\n", j,
                (LW2080_CTRL_LWLINK_GET_TL_INTEN_BIT(lwlinkErr.linkErrInfo[i].TLIntrEn, j) ? "Enabled" : "Disabled"));
        }

        for(j = 0; j < LW2080_CTRL_LWLINK_TL_ERRLOG_IDX_MAX; j++)
        {
            Printf(Tee::PriHigh, "    Error status bit for error %u : %s\n", j,
                (LW2080_CTRL_LWLINK_GET_TL_ERRLOG_BIT(lwlinkErr.linkErrInfo[i].TLErrlog, j) ? "True" : "False"));
        }

        Printf(Tee::PriHigh, " \n");
    }
    FOR_EACH_INDEX_IN_MASK_END;

    return rc;
}

//! \brief Cleanup function
//------------------------------------------------------------------------------
RC LWLINKTest::Cleanup()
{
    RC rc = OK;

    return OK;
}

JS_CLASS_INHERIT(LWLINKTest, RmTest, "Sw Test");
