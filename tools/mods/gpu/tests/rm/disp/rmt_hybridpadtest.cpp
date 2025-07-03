/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2011,2013-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/gpu.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/display.h"
#include "gpu/perf/powermgmtutil.h"
#include "core/include/disp_test.h"
#include "gpu/tests/rmtest.h"
#include "lwRmApi.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include <string> // Only use <> for built-in C++ stuff

#include "ctrl/ctrl0073.h"
#include "core/include/memcheck.h"

#define HYBRID_PAD_POWER_ON     0x1
#define HYBRID_PAD_POWER_DOWN   0x0

#define HYBRID_PAD_MODE_AUX     0x1
#define HYBRID_PAD_MODE_I2C     0x0

#define MAX_DCB_ENTRIES         16
#define ILWALID_VALUE           0xFF

class HybridPadsTest : public RmTest
{
public:
    HybridPadsTest();
    virtual ~HybridPadsTest();

    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    Display       *pDisplay = nullptr;
    DisplayIDs    supportedDisplays; // Which all displays are supported.
    DisplayIDs    connectedDisplays; // Which all displays are connected.
    LwU32         connectorIndex[MAX_DCB_ENTRIES] = {};// Store connector index of connected displays.
    LwU32         numHybridPads = 0;
    PowerMgmtUtil m_powerMgmtUtil;

    // Private interfaces
    RC TestHybridPadsConfguration();
    bool IsDPDisplayID(DisplayID Display);
    bool IsDisplayConnected(DisplayID Display);
    bool IsPartnerDispConnected(LwU32 connIdx);
    bool CheckPadPowerState(UINT32 index);
};

//! \brief HybridPadsTest constructor
//!
//! \sa Setup
//------------------------------------------------------------------------------
HybridPadsTest::HybridPadsTest()
{
    SetName("HybridPadsTest");
}

//! \brief HybridPadsTest destructor
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
HybridPadsTest::~HybridPadsTest()
{

}

//! \brief IsTestSupported will return true for G94 and above false otherwise
//------------------------------------------------------------------------------
string HybridPadsTest::IsTestSupported()
{
    if (Platform::GetSimulationMode() != Platform::Hardware)
        return "Supported only on HW";

    if (!GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_HYBRID_PADS))
        return "Don't have GPUSUB_SUPPORTS_HYBRID_PADS feature";

    return RUN_RMTEST_TRUE;
}

//! Will initialize all member variables and display H/W here. Also allocate
//! RM root.
//
//! \return OK, If all allocation are success.
//! \sa Run()
//------------------------------------------------------------------------------
RC HybridPadsTest::Setup()
{
    RC rc;
    LW0073_CTRL_DFP_GET_HYBRID_PADS_INFO_PARAMS dfpGetHybridPadInfo = {0} ;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    pDisplay = GetDisplay();

    CHECK_RC(pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));
    CHECK_RC(pDisplay->GetConnectors(&supportedDisplays));
    CHECK_RC(pDisplay->GetDetected(&connectedDisplays));

    for (UINT32 i = 0; i < MAX_DCB_ENTRIES; i++)
    {
        connectorIndex[i] = ILWALID_VALUE;
    }

    // Get the number of aux ports which have changed across chips.
    for (UINT32 i = 0; i < (UINT32)supportedDisplays.size(); i++)
    {
        dfpGetHybridPadInfo.displayId = (UINT32)supportedDisplays[i];
        if (pDisplay->RmControl(LW0073_CTRL_CMD_DFP_GET_HYBRID_PADS_INFO,
                         &dfpGetHybridPadInfo,
                         sizeof(LW0073_CTRL_DFP_GET_HYBRID_PADS_INFO_PARAMS)) != OK)
        {
            Printf(Tee::PriNormal, "TestHybridPadsConfguration: RmControl is failing.\n");
            return rc;
        }

        if (dfpGetHybridPadInfo.bUsesHybridPad)
        {
            numHybridPads = dfpGetHybridPadInfo.numAuxPorts;
            break;
        }
    }

    return rc;
}

//! Run will test hybrid pad's configuration
//! 1.After noremal Boot
//! 2.After S3 resume.
//! 3.After S4 resume.
//! 4.Also check for unused hybrid pads.
//------------------------------------------------------------------------------
RC HybridPadsTest::Run()
{
    RC rc;

    m_powerMgmtUtil.BindGpuSubdevice(GetBoundGpuSubdevice());

    // Test Hybrid pads Configuration after Boot.
    CHECK_RC(TestHybridPadsConfguration());

    // S3, S4 calls supported only on WinMods(BUILD_OS=win32) and WinMfgMods(BUILD_OS=winmfg).
    if ((Xp::GetOperatingSystem() == Xp::OS_WINDOWS) ||
        (Xp::GetOperatingSystem() == Xp::OS_WINMFG))
    {
        // Do suspend-resume
        CHECK_RC(m_powerMgmtUtil.PowerStateTransition(m_powerMgmtUtil.POWER_S3, 15));

        // After S3 resume check Hybrid pads configuration.
        CHECK_RC(TestHybridPadsConfguration());
    }
    return rc;
}

//! Cleanup
//-----------------------------------------------------------------------------
RC HybridPadsTest::Cleanup()
{
    return OK;
}

//-----------------------------------------------------------------------------
// Private Member Functions
//-----------------------------------------------------------------------------

//! This function will check following things
//! 1. Display uses Hybrid pad or not.
//! 2. Hybrid pad is power on/off.
//! 3. Hybrid pad is in I2C/Aux mode.
//-----------------------------------------------------------------------------
RC HybridPadsTest::TestHybridPadsConfguration()
{
    RC rc;
    LW0073_CTRL_DFP_GET_HYBRID_PADS_INFO_PARAMS dfpGetHybridPadInfo = {0} ;
    LW0073_CTRL_SPECIFIC_GET_CONNECTOR_DATA_PARAMS dataParams = {0};
    vector <bool>bUsedPads;
    bUsedPads.reserve(numHybridPads);
    UINT32 i = 0;

    for (i = 0; i < (UINT32)connectedDisplays.size(); i++)
    {
        dataParams.subDeviceInstance = 0;
        dataParams.displayId = connectedDisplays[i];
        rc = pDisplay->RmControl(
                    LW0073_CTRL_CMD_SPECIFIC_GET_CONNECTOR_DATA,
                    &dataParams, sizeof (dataParams));

        if (rc == OK)
        {
            connectorIndex[i] = dataParams.data[0].index;
        }
    }

    for (i = 0; i < (UINT32)supportedDisplays.size(); i++)
    {
        if (!supportedDisplays[i])
            continue;

        dfpGetHybridPadInfo.displayId = (UINT32)supportedDisplays[i];

        if (pDisplay->RmControl(LW0073_CTRL_CMD_DFP_GET_HYBRID_PADS_INFO,
                                     &dfpGetHybridPadInfo,
                                     sizeof(LW0073_CTRL_DFP_GET_HYBRID_PADS_INFO_PARAMS)) != OK)
        {
            Printf(Tee::PriNormal, "TestHybridPadsConfguration: RmControl is failing.\n");
        }

        if (dfpGetHybridPadInfo.bUsesHybridPad)
        {
            bUsedPads[dfpGetHybridPadInfo.physicalPort] = true;

            if (IsDisplayConnected((DisplayID)dfpGetHybridPadInfo.displayId))
            {
                if (dfpGetHybridPadInfo.bpadLwrrState == HYBRID_PAD_POWER_DOWN)
                {
                    Printf(Tee::PriNormal, "TestHybridPadsConfguration: Pad is not powered on but display is connected.\n");
                    rc = RC::DATA_MISMATCH;
                }

                if((IsDPDisplayID(supportedDisplays[i])) &&
                   (dfpGetHybridPadInfo.bPadLwrrMode == HYBRID_PAD_MODE_I2C))
                {
                    Printf(Tee::PriNormal, "TestHybridPadsConfguration: OD is displayPort but current mode is I2C.\n");
                    rc = RC::DATA_MISMATCH;
                }
                if(!(IsDPDisplayID(supportedDisplays[i])) &&
                    (dfpGetHybridPadInfo.bPadLwrrMode == HYBRID_PAD_MODE_AUX))
                {
                    Printf(Tee::PriNormal, "TestHybridPadsConfguration: OD is TMDS but current mode is DPAUX.\n");
                    rc = RC::DATA_MISMATCH;
                }
            }
            else
            {
                if (dfpGetHybridPadInfo.bpadLwrrState == HYBRID_PAD_POWER_ON)
                {
                    // Check if this display is a partner display of one of the connected displays.
                    dataParams.subDeviceInstance = 0;
                    dataParams.displayId = supportedDisplays[i];
                    rc = pDisplay->RmControl(
                                LW0073_CTRL_CMD_SPECIFIC_GET_CONNECTOR_DATA,
                                &dataParams, sizeof (dataParams));

                    if (IsPartnerDispConnected(dataParams.data[0].index))
                    {
                        continue;
                    }
                    else
                    {
                        Printf(Tee::PriNormal, "TestHybridPadsConfguration: Pad is powered on but display is not connected.\n");
                        rc = RC::DATA_MISMATCH;
                    }
                }
            }
        }
    }

    // Check if the unused hybrid pads are powered down or not.
    for (UINT32 j = 0; j < numHybridPads; j++)
    {
        if (bUsedPads[j]) continue;

        if (CheckPadPowerState(j) == HYBRID_PAD_POWER_ON)
        {
            Printf(Tee::PriNormal, "TestHybridPadsConfguration: Unused Pad is powered on.\n");
            rc = RC::DATA_MISMATCH;
        }
    }

    return rc;
}

//! IsDisplayConnected will return true if display is connected, false other wise.
//--------------------------------------------------------------------------------
bool HybridPadsTest::IsDisplayConnected(DisplayID Display)
{
    UINT32 index = 0;
    for ( ;index < (UINT32)connectedDisplays.size(); index++)
    {
        if (connectedDisplays[index] == Display)
        {
            return true;
        }
    }
    return false;
}

//! IsPartnerDispConnected will return true if the given connector index matches the connector index of the connected display, false other wise.
//-----------------------------------------------------------------------------------------------------------------------------------------------
bool HybridPadsTest::IsPartnerDispConnected(LwU32 connIdx)
{
    UINT32 index = 0;
    for ( ;index < MAX_DCB_ENTRIES; index++)
    {
        if (connectorIndex[index] == connIdx)
        {
            return true;
        }
    }
    return false;
}

//! IsDPDisplayID will return true if display is display port, false otherwise.
//--------------------------------------------------------------------------------
bool HybridPadsTest::IsDPDisplayID(DisplayID Display)
{
    // Ask RM which OR should be used (and protocol) for this Display
    LW0073_CTRL_SPECIFIC_OR_GET_INFO_PARAMS params;
    memset(&params, 0, sizeof(params));
    params.subDeviceInstance = pDisplay->GetOwningDisplaySubdevice()->GetSubdeviceInst();
    params.displayId = Display;
    if (pDisplay->RmControl(LW0073_CTRL_CMD_SPECIFIC_OR_GET_INFO,
                            &params, sizeof(params)) != OK)
    {
        return false;
    }

    return (params.type == LW0073_CTRL_SPECIFIC_OR_TYPE_SOR) &&
           (params.protocol == LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DP_A ||
            params.protocol == LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DP_B);
}

//! CheckPadPowerState will return true if pad is powered off, false otherwise.
//--------------------------------------------------------------------------------
bool HybridPadsTest::CheckPadPowerState(UINT32 index)
{
    LW0073_CTRL_CMD_SPECIFIC_GET_HYBRID_PAD_POWER_PARAMS dfpGetHybridPadPower = {0} ;

    dfpGetHybridPadPower.subDeviceInstance = 0;
    dfpGetHybridPadPower.physicalPort = index;

    if (pDisplay->RmControl(LW0073_CTRL_CMD_SPECIFIC_GET_HYBRID_PAD_POWER,
                                 &dfpGetHybridPadPower,
                                 sizeof(LW0073_CTRL_CMD_SPECIFIC_GET_HYBRID_PAD_POWER_PARAMS)) != OK)
    {
        Printf(Tee::PriNormal, "TestHybridPadsConfguration: RmControl is failing.\n");
    }

    return (dfpGetHybridPadPower.powerState & 0x1) ? HYBRID_PAD_POWER_DOWN : HYBRID_PAD_POWER_ON;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.  Don't worry about the details
//! here, you probably don't care.  :-)
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(HybridPadsTest, RmTest,
    "Test Hybrid Pad configuration");

