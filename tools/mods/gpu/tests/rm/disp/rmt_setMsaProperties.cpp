/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2009,2011,2015-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_setMsaProperties.cpp
//! \brief Sample test to verify sw programmable MSA:
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "core/include/display.h"
#include "core/include/xp.h"
#include "gpu/tests/rm/utility/crccomparison.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/display/evo_disp.h"

#include "core/include/display.h"
#include "disp/v02_02/dev_disp.h"

#include "class/cl0073.h"
#include "ctrl/ctrl0073.h"

#include "core/include/memcheck.h"

#define FINDMIN(a, b)          ((a) < (b) ? (a): (b))
#define arraySize(x) (sizeof(x)/sizeof(x[0]))

LW0073_CTRL_CMD_DP_SET_MSA_PROPERTIES_PARAMS msaTestdata[]=
{
    //msaTestdata[0]
    {
        0,          //subDeviceInstance
        0,          //displayId
        LW_TRUE,    //bEnableMSA
        LW_FALSE,   //bStereoPhaseIlwerse
        LW_FALSE,   //bCacheMsaOverrideForNextModeset

        {   //featureMask
            {0xFF, 0xFF}, //miscMask
            LW_TRUE,      //bRasterTotalHorizontal
            LW_TRUE,      //bRasterTotalVertical
            LW_TRUE,      //bActiveStartHorizontal
            LW_TRUE,      //bActiveStartVertical
            LW_TRUE,      //bSurfaceTotalHorizontal
            LW_TRUE,      //bSurfaceTotalVertical
            LW_TRUE,      //bSyncWidthHorizontal
            LW_TRUE,      //bSyncPolarityHorizontal
            LW_TRUE,      //bSyncHeightVertical
            LW_TRUE,      //bSyncPolarityVertical
            {LW_TRUE, LW_TRUE, LW_TRUE}, //bReservedEnable
        },
        {   //featureValues
            {0xAB, 0xAB},   //misc                    :
            0xCDEF,         //rasterTotalHorizontal
            0x1234,         //rasterTotalVertical
            0x5678,         //activeStartHorizontal
            0x9ABC,         //activeStartVertical
            0xCDEF,         //surfaceTotalHorizontal
            0x8754,         //surfaceTotalVertical
            0x2564,         //syncWidthHorizontal
            LW0073_CTRL_CMD_DP_MSA_PROPERTIES_SYNC_POLARITY_LOW,    //syncPolarityHorizontal
            0x1547,         //syncHeightVertical
            LW0073_CTRL_CMD_DP_MSA_PROPERTIES_SYNC_POLARITY_HIGH,   //syncPolarityVertical
            {0xAB, 0xAB, 0xAB},     //reserved
        },
        NULL        //pFeatureDebugValues
    }

    //msaTestdata[1]
    ,{
        0,          //subDeviceInstance
        0,          //displayId
        LW_TRUE,    //bEnableMSA
        LW_FALSE,   //bStereoPhaseIlwerse
        LW_FALSE,   //bCacheMsaOverrideForNextModeset

        {   //featureMask
            {0xFF, 0xFF}, //miscMask
            LW_TRUE,      //bRasterTotalHorizontal
            LW_TRUE,      //bRasterTotalVertical
            LW_TRUE,      //bActiveStartHorizontal
            LW_TRUE,      //bActiveStartVertical
            LW_TRUE,      //bSurfaceTotalHorizontal
            LW_TRUE,      //bSurfaceTotalVertical
            LW_TRUE,      //bSyncWidthHorizontal
            LW_TRUE,      //bSyncPolarityHorizontal
            LW_TRUE,      //bSyncHeightVertical
            LW_TRUE,      //bSyncPolarityVertical
            {LW_TRUE, LW_TRUE, LW_TRUE}, //bReservedEnable
        },
        {   //featureValues
            {0xAB, 0xAB},   //misc                    :
            0xC581,         //rasterTotalHorizontal
            0x1845,         //rasterTotalVertical
            0x5841,         //activeStartHorizontal
            0x9BFC,         //activeStartVertical
            0xC4A1,         //surfaceTotalHorizontal
            0x8545,         //surfaceTotalVertical
            0x2981,         //syncWidthHorizontal
            LW0073_CTRL_CMD_DP_MSA_PROPERTIES_SYNC_POLARITY_HIGH,    //syncPolarityHorizontal
            0x1584,         //syncHeightVertical
            LW0073_CTRL_CMD_DP_MSA_PROPERTIES_SYNC_POLARITY_LOW,   //syncPolarityVertical
            {0xAB, 0xAB, 0xAB},     //reserved
        },
        NULL        //pFeatureDebugValues
    }
};

class SetMsaProperties : public RmTest
{
public:
    SetMsaProperties();
    virtual ~SetMsaProperties();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(protocol, string);   //Config protocol through cmdline
    SETGET_PROP(rastersize, string); //Config raster size through cmdline
    SETGET_PROP(onlyConnectedDisplays,bool);

private:
    DisplayID m_displayId;
    Display*  m_pDisplay;
    bool      m_RunningOnWin32Disp;
    bool      m_onlyConnectedDisplays;
    string    m_protocol;
    string    m_rastersize;

    RC MSAtest();

    RC verifyDebugValues
        (LW0073_CTRL_DP_MSA_PROPERTIES_MASK* pMask
        , LW0073_CTRL_DP_MSA_PROPERTIES_VALUES* pValues
        , LW0073_CTRL_DP_MSA_PROPERTIES_VALUES* pDebugValues);
};

//!
//! @brief SetMsaProperties default constructor.
//!
//! Sets the test name, that't it.
//!
//! @param[in]  NONE
//! @param[out] NONE
//-----------------------------------------------------------------------------
SetMsaProperties::SetMsaProperties() : m_displayId(0)
{
    SetName("SetMsaProperties");
    m_pDisplay = GetDisplay();
    m_RunningOnWin32Disp = false;
    m_onlyConnectedDisplays = false;
    m_protocol = "DP_A";
    m_rastersize = "";
}

//!
//! @brief SetMsaProperties default destructor.
//!
//! Placeholder : Doesn't do anything specific.
//!
//! @param[in]  NONE
//! @param[out] NONE
//-----------------------------------------------------------------------------
SetMsaProperties::~SetMsaProperties()
{

}

//!
//! @brief IsTestSupported(), Looks for whether test can execute in current elw.
//!
//! Checks whether we're running in Win32Display infra. This test lwrrently
//! does not support Win32Display infra.
//!
//! @param[in]  NONE
//! @param[out] NONE
//!
//! @returns    True if the test can be run in the current environment,
//!             false otherwise.
//-----------------------------------------------------------------------------
string SetMsaProperties::IsTestSupported()
{
    DisplayIDs    Detected;
    RC            rc = OK;

    if (m_protocol.compare("DP_A") != 0)
    {
        return "This test supports DP_A only";
    }

    if (m_pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS) != OK)
    {
        return "InitializeDisplayHW failed. Can not continue with the test";
    }

    m_RunningOnWin32Disp = (m_pDisplay->GetDisplayClassFamily() == Display::WIN32DISP);

    if (m_RunningOnWin32Disp)
    {
        return "Can not run on WIN32DISP";
    }

    if (m_pDisplay->GetDetected(&Detected) != OK)
    {
        return "GetDetected failed. Can not continue with the test";
    }

    Printf(Tee::PriHigh, "%s: Requested protocol for running the test: %s\n"
                       , __FUNCTION__, m_protocol.c_str());

    for (UINT32 i = 0; i < Detected.size(); i++)
    {
        string Protocol = "";

        if(DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay, Detected[i], m_protocol))
        {
            m_displayId = Detected[i];
            break;
        }
    }

    //If !m_onlyConnectedDisplays then consider fake displays too
    if((!m_onlyConnectedDisplays) && (m_displayId == 0))
    {
        DisplayIDs supportedDisplays;

        CHECK_RC_CLEANUP(m_pDisplay->GetConnectors(&supportedDisplays));

        for (LwU32 i = 0; i < supportedDisplays.size(); i++)
        {
            if (!m_pDisplay->IsDispAvailInList(supportedDisplays[i], Detected))
            {
                if(DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay, supportedDisplays[i], m_protocol))
                {
                    m_displayId = supportedDisplays[i];
                    CHECK_RC_CLEANUP(DTIUtils::EDIDUtils::SetLwstomEdid(m_pDisplay, m_displayId, false));
                    break;
                }
            }
        }
    }
    if (!m_displayId)
        return "Could not find any attached display device that supports requested protocol";
    return RUN_RMTEST_TRUE;

   Cleanup:
       if (rc == OK)
           return RUN_RMTEST_TRUE;
       return rc.Message();
}

//!
//! @brief Setup(): Just doing Init from Js, nothing else.
//!
//! @param[in]  NONE
//! @param[out] NONE
//!
//! @returns    OK if init passed, specific RC if failed.
//-----------------------------------------------------------------------------
RC SetMsaProperties::Setup()
{
    RC rc;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());
    return rc;
}

//!
//! @brief Run(): Used generally for placing all the testing stuff.
//!
//! Runs two tests:
//!     1. MonitorOffTest
//!
//! @param[in]  NONE
//! @param[out] NONE
//!
//! @returns    OK if all tests passed, specific RC if failed.
//-----------------------------------------------------------------------------
RC SetMsaProperties::Run()
{
    RC rc;

    CHECK_RC(MSAtest());

    return rc;
}

//!
//! @brief Cleanup(): does nothing for this simple test.
//!
//! @param[in]  NONE
//! @param[out] NONE
//!
//! @returns    OK
//-----------------------------------------------------------------------------
RC SetMsaProperties::Cleanup()
{
    return OK;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//!
//! @brief MonitorOffTest(): Demonstates the use of TurnScreenOff() API.
//!
//! Turns monitor off and then on using TurnScreenOff() API.
//!
//! @param[in]  NONE
//! @param[out] NONE
//!
//! @returns    OK if all tests passed, specific RC if failed.
//-----------------------------------------------------------------------------
RC SetMsaProperties::MSAtest()
{

    // This is XP Style Mon Off/On Test ..

    RC            rc    = OK;
    UINT32        BaseWidth;
    UINT32        BaseHeight;
    UINT32        Depth = 32;
    UINT32        RefreshRate;
    bool          useSmallRaster = false;

    LwU32 displaySubdevice;
    LW0073_CTRL_CMD_DP_SET_MSA_PROPERTIES_PARAMS *msaPropertiesParams;
    LW0073_CTRL_DP_MSA_PROPERTIES_VALUES        featureDebugValues;

    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        Platform::EscapeWrite("Disp_AllOrs_EnableImageOutput",0,4,1);
    }

    if (m_rastersize.compare("") == 0)
    {
        useSmallRaster = (Platform::GetSimulationMode() != Platform::Hardware);
    }
    else
    {
        useSmallRaster = (m_rastersize.compare("small") == 0);
        if (!useSmallRaster && m_rastersize.compare("large") != 0)
        {
            // Raster size param must be either "small" or "large".
            Printf(Tee::PriHigh, "%s: Bad raster size argument: \"%s\". Treating as \"large\"\n"
                                , __FUNCTION__, m_rastersize.c_str());
        }
    }

    if (useSmallRaster)
    {
        // This is for Fmodel and RTL which are slow in producing frames
        BaseWidth    = 160;
        BaseHeight   = 120;
        RefreshRate  = 60;
    }
    else
    {
        BaseWidth    = 1024;
        BaseHeight   = 768;
        RefreshRate  = 60;
    }

    Printf(Tee::PriHigh, "Doing Modeset On DisplayID %0x",(UINT32)m_displayId);

    // do modeset on the current displayId
    CHECK_RC(m_pDisplay->SetMode(m_displayId,
                               BaseWidth,
                               BaseHeight,
                               Depth,
                               RefreshRate));

    for(unsigned i = 0; i < arraySize(msaTestdata) ; i++)
    {
        msaPropertiesParams = &(msaTestdata[i]);
        msaPropertiesParams->pFeatureDebugValues = &featureDebugValues;
        CHECK_RC(m_pDisplay->GetDisplaySubdeviceInstance(&displaySubdevice));
        msaPropertiesParams->displayId  = (UINT32)m_displayId ;
        msaPropertiesParams->subDeviceInstance = displaySubdevice;

        CHECK_RC(m_pDisplay->RmControl(LW0073_CTRL_CMD_DP_SET_MSA_PROPERTIES,
            msaPropertiesParams, sizeof(LW0073_CTRL_CMD_DP_SET_MSA_PROPERTIES_PARAMS)));

        CHECK_RC(verifyDebugValues(&(msaPropertiesParams->featureMask)
                                    , &(msaPropertiesParams->featureValues)
                                    , msaPropertiesParams->pFeatureDebugValues));
    }
    return rc;
}

RC SetMsaProperties::verifyDebugValues
    (LW0073_CTRL_DP_MSA_PROPERTIES_MASK* pMask
    , LW0073_CTRL_DP_MSA_PROPERTIES_VALUES* pValues
    , LW0073_CTRL_DP_MSA_PROPERTIES_VALUES* pDebugValues)
{
    if(    0 != ((pMask->miscMask[0] & pValues->misc[0]) ^ (pMask->miscMask[0] & pDebugValues->misc[0]))
        || 0 != ((pMask->miscMask[1] & pValues->misc[1]) ^ (pMask->miscMask[1] & pDebugValues->misc[1]))
      )
        return RC::GOLDEN_VALUE_MISCOMPARE;

    if(LW_TRUE == pMask->bActiveStartHorizontal &&
        0 != (pValues->activeStartHorizontal ^ pDebugValues->activeStartHorizontal))
        return RC::GOLDEN_VALUE_MISCOMPARE;

    if(LW_TRUE == pMask->bActiveStartVertical &&
        0 != (pValues->activeStartVertical ^ pDebugValues->activeStartVertical))
        return RC::GOLDEN_VALUE_MISCOMPARE;

    if(LW_TRUE == pMask->bRasterTotalHorizontal &&
        0 != (pValues->rasterTotalHorizontal ^ pDebugValues->rasterTotalHorizontal))
        return RC::GOLDEN_VALUE_MISCOMPARE;

    if(LW_TRUE == pMask->bRasterTotalVertical &&
        0 != (pValues->rasterTotalVertical ^ pDebugValues->rasterTotalVertical))
        return RC::GOLDEN_VALUE_MISCOMPARE;

    if(LW_TRUE == pMask->bSurfaceTotalHorizontal &&
        0 != (pValues->surfaceTotalHorizontal ^ pDebugValues->surfaceTotalHorizontal))
        return RC::GOLDEN_VALUE_MISCOMPARE;

    if(LW_TRUE == pMask->bSurfaceTotalVertical &&
        0 != (pValues->surfaceTotalVertical ^ pDebugValues->surfaceTotalVertical))
        return RC::GOLDEN_VALUE_MISCOMPARE;

    if(LW_TRUE == pMask->bSyncHeightVertical &&
        0 != (pValues->syncHeightVertical ^ pDebugValues->syncHeightVertical))
        return RC::GOLDEN_VALUE_MISCOMPARE;

    if(LW_TRUE == pMask->bSyncPolarityHorizontal &&
        0 != (pValues->syncPolarityHorizontal ^ pDebugValues->syncPolarityHorizontal))
        return RC::GOLDEN_VALUE_MISCOMPARE;

    if(LW_TRUE == pMask->bSyncPolarityVertical &&
        0 != (pValues->syncPolarityVertical ^ pDebugValues->syncPolarityVertical))
        return RC::GOLDEN_VALUE_MISCOMPARE;

    if(LW_TRUE == pMask->bSyncWidthHorizontal &&
        0 != (pValues->syncWidthHorizontal ^ pDebugValues->syncWidthHorizontal))
        return RC::GOLDEN_VALUE_MISCOMPARE;

    if(LW_TRUE == pMask->bReservedEnable[0] &&
        0 != (pValues->reserved[0] ^ pDebugValues->reserved[0]))
        return RC::GOLDEN_VALUE_MISCOMPARE;

    if(LW_TRUE == pMask->bReservedEnable[1] &&
        0 != (pValues->reserved[1] ^ pDebugValues->reserved[1]))
        return RC::GOLDEN_VALUE_MISCOMPARE;

    if(LW_TRUE == pMask->bReservedEnable[2] &&
        0 != (pValues->reserved[2] ^ pDebugValues->reserved[2]))
        return RC::GOLDEN_VALUE_MISCOMPARE;

    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(SetMsaProperties, RmTest,
    "Simple test to demonstrate usage of monitor off APIs");

CLASS_PROP_READWRITE(SetMsaProperties, protocol, string,
                     "OR protocol to use for the test");
CLASS_PROP_READWRITE(SetMsaProperties, rastersize, string,
                     "Desired raster size (small/large)");
CLASS_PROP_READWRITE(SetMsaProperties, onlyConnectedDisplays, bool,
                     "run on only connected displays, default = 0");

