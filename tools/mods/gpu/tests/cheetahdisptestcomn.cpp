/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "tegradisptestcomn.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"

//-----------------------------------------------------------------------------
//! \brief Constructor
TegraDispTestComn::TegraDispTestComn()
 :  m_CachedDisplay(0)
   ,m_CachedConType(TegraDisplay::CT_NUM_TYPES)
   ,m_CachedDsiId(TegraDisplay::DSI_ILWALID)
   ,m_CachedSkippedConn(false)
   ,m_UseRealDisplays(false)
   ,m_OrigDisplay(0)
   ,m_RealDisplayDisabled(false)
   ,m_DispWidth(0)
   ,m_DispHeight(0)
   ,m_DispDepth(0)
   ,m_DispRefresh(0)
{
}

//-----------------------------------------------------------------------------
//! \brief Print the test properties
//!
//! \return OK if successful, not OK otherwise
void TegraDispTestComn::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);
    Printf(pri, "TegraDispTestComn Js Properties:\n");
    string skippedConnTypes = "none";
    if (!m_SkippedConnectionTypes.empty())
    {
        set<string>::iterator pSkipped = m_SkippedConnectionTypes.begin();
        skippedConnTypes = "";
        for ( ;pSkipped != m_SkippedConnectionTypes.end(); pSkipped++)
        {
            skippedConnTypes += *pSkipped;
            skippedConnTypes += " ";
        }
    }
    Printf(pri, "\tSkipConnectionTypes:\t\t%s\n", skippedConnTypes.c_str());
}

//----------------------------------------------------------------------------
//! \brief Initialize test properties from JS
//!
//! \return OK if successful, not OK otherwise
//!
RC TegraDispTestComn::InitFromJs()
{
    RC rc;

    // Call base-class function (for TestConfiguration and Golden properties).
    CHECK_RC(GpuTest::InitFromJs());

    // Initialize SkipConnectionTypes by getting the actual propery from JS
    // ratherthan by exporting a C++ JS property.  This is due to the nature of
    // how JS properties are copied onto the test object from test arguments
    // properties / arrays are copied individually rather than an entire
    // array assignment at once (what the C++ interface expects)
    JsArray jsaSkipConnTypes;
    JavaScriptPtr pJs;
    rc = pJs->GetProperty(GetJSObject(),
                          "SkipConnectionTypes",
                          &jsaSkipConnTypes);
    if (OK == rc)
    {
        for (size_t i = 0; i < jsaSkipConnTypes.size(); ++i)
        {
            string tmp;
            (void) pJs->FromJsval(jsaSkipConnTypes[i], &tmp);
            m_SkippedConnectionTypes.insert(tmp);
        }
    }
    else if (RC::ILWALID_OBJECT_PROPERTY == rc)
    {
        rc.Clear();
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Add a test mode
//!
//! \param testMode      : Test mode to add
void TegraDispTestComn::AddMode(const TestMode testMode)
{
    m_TestModes.push_back(testMode);
}

//-----------------------------------------------------------------------------
//! \brief Set a mode in a cheetah display test
//!
//! \param pDisplay        : Pointer to display to set the mode
//! \param head            : Head to set the mode
//! \param display         : Display to set
//! \param testMode        : Test mode to set
//! \param pOverrideOrInfo : Pointer to OR override RAII class
//! \param pOverrideEdid   : Pointer to EDID override RAII class
//! \param pModeSkipped    : Returned indication if the mode is skipped
//!
//! \return OK if successful and no new underflows, not OK otherwise
RC TegraDispTestComn::SetMode
(
    TegraDisplay *pDisplay,
    UINT32 head,
    UINT32 display,
    const TestMode &testMode,
    OverrideOrInfo *pOverrideOrInfo,
    OverrideEdid * pOverrideEdid,
    bool *pModeSkipped
)
{
    RC rc;
    const Tee::Priority pri = GetVerbosePrintPri();
    TegraDisplay::ConnectionType conType;
    TegraDisplay::DsiId dsiId = TegraDisplay::DSI_ILWALID;
    bool bConnectionSkipped;
    CHECK_RC(GetConnectionType(pDisplay,
                               display,
                               &conType,
                               &dsiId,
                               &bConnectionSkipped));

    CHECK_RC(pDisplay->Select(display));

    if (!m_UseRealDisplays)
    {
        if (TegraDisplay::CT_DSI == conType)
        {
            if ((dsiId == TegraDisplay::DSI_AB) != testMode.mode.dsiInfo.bGanged)
            {
                Printf(pri,
                      "Skipping %sGanged DSI mode on %sGanged "
                      "connector on head %d Display 0x%08x\n",
                      testMode.mode.dsiInfo.bGanged ? "" : "Non-",
                      (dsiId == TegraDisplay::DSI_AB) ? "" : "Non-",
                      head,
                      display);
                *pModeSkipped = true;
                return rc;
            }
            if (pOverrideOrInfo)
            {
                CHECK_RC(pOverrideOrInfo->SetOverride(testMode.mode.dsiInfo));
            }
        }

        if ((TegraDisplay::CT_EDP == conType) && pOverrideOrInfo)
        {
            CHECK_RC(pOverrideOrInfo->SetOverride(testMode.mode.dpInfo));
        }

        if (pOverrideOrInfo &&
            ((TegraDisplay::CT_DSI == conType) ||
             (TegraDisplay::CT_EDP == conType) ||
             (TegraDisplay::CT_LCD == conType)))
        {
            MASSERT(pOverrideEdid);
            if (pOverrideEdid == nullptr)
                return RC::SOFTWARE_ERROR;

            CHECK_RC(pOverrideEdid->SetOverride(
                        pDisplay, display, testMode.mode.edid));
        }

        TegraDisplay::OutputPixelEncoding ope = TegraDisplay::OutputPixelEncoding(
                testMode.mode.opeFormat,
                testMode.mode.opeYuvBpp);
        if ((testMode.mode.opeFormat != Display::OpeFmt::RGB) &&
            !pDisplay->IsYUVFmtSupported(conType, ope))
        {
            Printf(Tee::PriError,
                  "YUV native modes provided: %s are not supported for connection type %s\n",
                  StringifyOPE(testMode.mode.opeFormat, testMode.mode.opeYuvBpp).c_str(),
                  pDisplay->ConnectionTypeToStr(conType));
            *pModeSkipped = true;
            return RC::MODE_NOT_SUPPORTED;
        }
    }
    bool bModePossible = false;
    CHECK_RC(pDisplay->IsModePossible(display,
                                      testMode.mode.width,
                                      testMode.mode.height,
                                      0,
                                      testMode.mode.refreshRate,
                                      testMode.mode.bInterlaced,
                                      TegraDisplay::OutputPixelEncoding (testMode.mode.opeFormat, testMode.mode.opeYuvBpp),
                                      &bModePossible));

// TODO: Move CheetAh display tests from ONLY_ARM to TEGRA_MODS builds
#ifdef TEGRA_MODS
    // Bug 1858552:
    // Due to a DTB EDID limiation in driver > 128 bytes will not get
    // parsed correctly which means HDMI 2.0 mode information in HF-VSDB
    // or any SCDC related bits cannot get parsed
    // With run-time EDID faking we can fake a 4K EDID via sysfs that does
    // not have this limitation
    // Don't allow 4K@60Hz unless run-time EDID faking is present
    if ((TegraDisplay::CT_HDMI == conType) &&
        ((testMode.mode.width == 3840) || (testMode.mode.width == 4096)) &&
        (testMode.mode.height == 2160) &&
        (testMode.mode.refreshRate == 60) &&
        !TegraDisplayLwDc::IsDynamicEdidFakingSupported(conType) &&
        (TegraDisplay::TEGRA_LWDISPLAY == pDisplay->GetDisplayArchitecture()))
    {
        Printf(Tee::PriWarn, "Cannot support 4K@60Hz on HDMI (use run-time EDID faking)!\n");
        bModePossible = false;
    }
#endif

    if (!bModePossible)
    {
        Printf(pri,
               "Skipping %s Resolution %uX%u%s %u Hz with %s on Head %u "
               "Display 0x%08x (mode not possible)\n",
                pDisplay->ConnectionTypeToStr(conType),
                testMode.mode.width, testMode.mode.height,
                testMode.mode.bInterlaced ? "i" : "",
                testMode.mode.refreshRate,
                StringifyOPE(testMode.mode.opeFormat, testMode.mode.opeYuvBpp).c_str(),
                head, display);
        *pModeSkipped = true;
        return rc;
    }

    string connTypeString;
    CHECK_RC(pDisplay->ConnectionTypeToName(conType, dsiId, &connTypeString));
    Printf(pri,
           "Selecting %s Resolution %uX%u%s %u Hz with %s on Head %u "
           "Display 0x%08x\n",
            connTypeString.c_str(),
            testMode.mode.width, testMode.mode.height,
            testMode.mode.bInterlaced ? "i" : "",
            testMode.mode.refreshRate,
            StringifyOPE(testMode.mode.opeFormat, testMode.mode.opeYuvBpp).c_str(),
            head, display);

    TegraDisplay::SetModeAdditionalParams params(testMode.mode.bInterlaced,
                                                 TegraDisplay::OutputPixelEncoding (testMode.mode.opeFormat, testMode.mode.opeYuvBpp));
    CHECK_RC(pDisplay->SetMode(display,
                               testMode.mode.width,
                               testMode.mode.height,
                               0,
                               testMode.mode.refreshRate,
                               params));
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Check the display to see if any new underflows have oclwrred
//!
//! \param pDisplay   : Pointer to display to check
//! \param head       : Head to check
//! \param origCounts : Original underflow counts
//!
//! \return OK if successful and no new underflows, not OK otherwise
RC TegraDispTestComn::CheckUnderflows
(
    TegraDisplay *pDisplay,
    UINT32 head,
    const TegraDisplay::UnderflowCounts& origCounts
)
{
    RC rc;
    TegraDisplay::UnderflowCounts newUnderflows;
    CHECK_RC(pDisplay->GetUnderflowCounts(head, &newUnderflows));

    if (newUnderflows != origCounts)
    {
        TegraDisplay::UnderflowCounts::const_iterator ufIt =
                newUnderflows.begin();
        for ( ; ufIt != newUnderflows.end(); ++ufIt)
        {
            TegraDisplay::UnderflowCounts::const_iterator prevIt =
                    origCounts.find(ufIt->first);
            const UINT32 prevUf = (prevIt != origCounts.end())
                                        ? prevIt->second : 0;
            if (ufIt->second == prevUf)
                continue;
            const string name =
                    (ufIt->first == TegraDisplay::WIN_HWLWRSOR)
                    ? "HW cursor"
                            : Utility::StrPrintf("window %c",
                                    pDisplay->GetWindowName(head, ufIt->first));
            const UINT32 count = ufIt->second - prevUf;
            const string countStr = count > 0
                    ? Utility::StrPrintf("%u", count) : "some";
            Printf(Tee::PriHigh, "Found %s underflows on %s (%d, %d)\n",
                                 countStr.c_str(), name.c_str(), ufIt->second, prevUf);
        }
        return RC::HW_STATUS_ERROR;
    }

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Get the connection information for a display
//!
//! \param pDisplay  : Pointer to display to get the connection type
//! \param display   : Display to get connection for
//! \param pConType  : Pointer to returned connection type
//! \param pDsiId    : Pointer to returned DSI Id for DSI connections
//! \param pbSkipped : Pointer to returned indicator if connection should be
//!                    skipped
//!
//! \return OK if successful, not OK otherwise
RC TegraDispTestComn::GetConnectionType
(
    TegraDisplay                 *pDisplay,
    UINT32                        display,
    TegraDisplay::ConnectionType *pConType,
    TegraDisplay::DsiId          *pDsiId,
    bool                         *pbSkipped
)
{
    RC rc;
    if (m_CachedDisplay != display)
    {
        CHECK_RC(pDisplay->GetConnectionType(display, &m_CachedConType));
        if (m_CachedConType == TegraDisplay::CT_DSI)
        {
            CHECK_RC(pDisplay->GetDsiId(display, &m_CachedDsiId));
        }

        switch (m_CachedConType)
        {
            case TegraDisplay::CT_HDMI:
                m_CachedSkippedConn =
                    m_SkippedConnectionTypes.count("hdmi") != 0;
                break;
            case TegraDisplay::CT_LCD:
                m_CachedSkippedConn =
                    m_SkippedConnectionTypes.count("lcd") != 0;
                break;
            case TegraDisplay::CT_EDP:
                m_CachedSkippedConn =
                    m_SkippedConnectionTypes.count("edp") != 0;
                break;
            case TegraDisplay::CT_DSI:
                switch (m_CachedDsiId)
                {
                    case TegraDisplay::DSI_A:
                        m_CachedSkippedConn =
                            m_SkippedConnectionTypes.count("dsia") != 0;
                        break;
                    case TegraDisplay::DSI_B:
                        m_CachedSkippedConn =
                            m_SkippedConnectionTypes.count("dsib") != 0;
                        break;
                    case TegraDisplay::DSI_AB:
                        m_CachedSkippedConn =
                            m_SkippedConnectionTypes.count("dsiab") != 0;
                        break;
                    default:
                        MASSERT(!"Unknown DSI type");
                        return RC::SOFTWARE_ERROR;
                }
                break;
            default:
                MASSERT(!"Unknown connection type");
                return RC::SOFTWARE_ERROR;
        }
        m_CachedDisplay = display;
    }

    if (nullptr != pConType)
        *pConType = m_CachedConType;
    if (nullptr != pDsiId && m_CachedConType == TegraDisplay::CT_DSI)
        *pDsiId = m_CachedDsiId;
    if (nullptr != pbSkipped)
        *pbSkipped = m_CachedSkippedConn;
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Setup the display
//!
//! \param pDisplay  : Pointer to display to setup
//!
//! \return OK if successful, not OK otherwise
RC TegraDispTestComn::SetupDisplay()
{
    RC rc;
    TegraDisplay *pDisplay = GetDisplay()->GetTegraDisplay();

    // Save selected display
    m_OrigDisplay = pDisplay->Selected();

    if (OK != pDisplay->GetMode(m_OrigDisplay, &m_DispWidth,
                                &m_DispHeight, &m_DispDepth,
                                &m_DispRefresh))
    {
        // Get a safe mode if the real mode cannot be retrieved
        GpuTestConfiguration *pTestConfig = GetTestConfiguration();
        m_DispWidth   = pTestConfig->DisplayWidth();
        m_DispHeight  = pTestConfig->DisplayHeight();
        m_DispDepth   = pTestConfig->DisplayDepth();
        m_DispRefresh = pTestConfig->RefreshRate();
    }

    if (m_UseRealDisplays)
    {
        vector<TestMode> realTestModes;

        UINT32 connected;
        bool bHasHdmi = false;
        CHECK_RC(pDisplay->GetConnectors(&connected));
        while (connected && !bHasHdmi)
        {
            TegraDisplay::ConnectionType conType;
            UINT32 singleDisplay = connected & ~(connected - 1);
            UINT32 nativeWidth, nativeHeight, nativeRefresh;

            connected ^= singleDisplay;
            CHECK_RC(pDisplay->GetConnectionType(singleDisplay, &conType));
            CHECK_RC(pDisplay->GetNativeResolution(singleDisplay,
                        &nativeWidth, &nativeHeight, &nativeRefresh));
            TestMode newMode;
            newMode.connection = conType;
            newMode.mode.width       = nativeWidth;
            newMode.mode.height      = nativeHeight;
            newMode.mode.depth       = 32;
            newMode.mode.refreshRate = nativeRefresh;
            newMode.mode.bInterlaced = false;
            realTestModes.push_back(newMode);
        }
        m_TestModes = realTestModes;
    }
    else
    {
        // Turn off display so it is not damaged by resolution changes
        CHECK_RC(pDisplay->EnableSimulatedDisplays(true));
        m_RealDisplayDisabled = true;
    }

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Cleanup the display
//!
//! \param pDisplay  : Pointer to display to setup
//!
//! \return OK if successful, not OK otherwise
RC TegraDispTestComn::CleanupDisplay()
{
    StickyRC rc;
    Display *pDisplay = GetDisplay();

    if (pDisplay && m_OrigDisplay)
    {
        MASSERT(pDisplay->GetDisplayClassFamily() == Display::TEGRA_ANDROID);

        if (!m_UseRealDisplays)
        {
            rc = pDisplay->GetTegraDisplay()->RestoreDefaultHeadRouting();
        }

        // Need to disable simulated displays before setting original
        // display/mode
        if (m_RealDisplayDisabled)
            rc = pDisplay->GetTegraDisplay()->EnableSimulatedDisplays(false);

        UINT32 detectedDsiDisplays = pDisplay->GetTegraDisplay()->GetAllDSIConnectors();
        bool isDefaultDsiFaked = (detectedDsiDisplays & m_OrigDisplay) &&
                                 pDisplay->GetTegraDisplay()->IsDsiPanelFaked();

        // If default display is faked (or) DSI fake panel present, avoid restore modeset
        if (!pDisplay->GetTegraDisplay()->IsDisplayDefaultAndFaked(m_OrigDisplay) &&
            !isDefaultDsiFaked)
        {
            rc = pDisplay->Select(m_OrigDisplay);

            // Must force a mode during cleanup so that for the default panel it
            // will restore safe settings before enabling the LCD (e.g. force
            // re-read of overridden dsi info block in the case of DSI)
            rc = pDisplay->SetMode(m_DispWidth, m_DispHeight,
                                   m_DispDepth, m_DispRefresh);
        }

        if (m_RealDisplayDisabled)
        {
            m_RealDisplayDisabled = false;
        }
    }
    return rc;
}

/*static*/string TegraDispTestComn::StringifyOPE(Display::OpeFmt opeFormat, Display::OpeYuvBpp opeYuvBpp)
{
    string strOpeFmt;
    string strYuvBpp;
    switch (opeFormat)
    {
        case Display::OpeFmt::YUV420: strOpeFmt = "yuv_420";
                                          break;
        case Display::OpeFmt::YUV422: strOpeFmt = "yuv_422";
                                          break;
        case Display::OpeFmt::YUV444: strOpeFmt = "yuv_444";
                                          break;
        case Display::OpeFmt::RGB:    strOpeFmt = "rgb";
                                          break;
        default:
             //Expect to find missing modes in dev cycle.So just adding MASSERT
             MASSERT(!"Unknown Mode");
             return "";
    }

    switch (opeYuvBpp)
    {
        case Display::OpeYuvBpp::BPP_24: strYuvBpp = "24bpp";
                                                 break;
        case Display::OpeYuvBpp::BPP_30: strYuvBpp = "30bpp";
                                                 break;
        case Display::OpeYuvBpp::BPP_36: strYuvBpp = "36bpp";
                                                 break;
        case Display::OpeYuvBpp::BPP_48: strYuvBpp = "48bpp";
                                                 break;
        case Display::OpeYuvBpp::BPP_NONE: strYuvBpp = "";
                                                 break;
        default:
            MASSERT(!"Unknown BPP");
            return "";
    }
    return (strOpeFmt + " " + strYuvBpp);
}

//-----------------------------------------------------------------------------
//! \brief Constructor
TegraDispTestComn::OverrideOrInfo::OverrideOrInfo()
: m_OvrState(OVR_STATE_NONE)
{
}

//-----------------------------------------------------------------------------
//! \brief Destrutor : Restores DSI info to original settings
TegraDispTestComn::OverrideOrInfo::~OverrideOrInfo()
{
    if (m_OvrState == OVR_STATE_DSI)
    {
        CheetAh::SocPtr()->SetDsiInfo(m_DsiInfo);
    }
    else if (m_OvrState == OVR_STATE_DP)
    {
        CheetAh::SocPtr()->SetDpInfo(m_DpInfo);
    }
}

//-----------------------------------------------------------------------------
//! \brief Override the DSI info
//!
//! \param dsiInfo : New DSI info to use
//!
//! \return OK if successful, not OK otherwise
RC TegraDispTestComn::OverrideOrInfo::SetOverride
(
    const CheetAh::SocDevice::DsiInfo& dsiInfo
)
{
    if (m_OvrState != OVR_STATE_NONE)
    {
        Printf(Tee::PriHigh, "OR info already overridden!\n");
        return RC::SOFTWARE_ERROR;
    }
    CheetAh::SocPtr()->GetDsiInfo(&m_DsiInfo);
    CheetAh::SocPtr()->SetDsiInfo(dsiInfo);
    m_OvrState = OVR_STATE_DSI;
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Override the DP info
//!
//! \param dsiInfo : New DP info to use
//!
//! \return OK if successful, not OK otherwise
RC TegraDispTestComn::OverrideOrInfo::SetOverride
(
    const CheetAh::SocDevice::DpInfo& dpInfo
)
{
    if (m_OvrState != OVR_STATE_NONE)
    {
        Printf(Tee::PriHigh, "OR info already overridden!\n");
        return RC::SOFTWARE_ERROR;
    }
    CheetAh::SocPtr()->GetDpInfo(&m_DpInfo);
    CheetAh::SocPtr()->SetDpInfo(dpInfo);
    m_OvrState = OVR_STATE_DP;
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Constructor
TegraDispTestComn::OverrideEdid::OverrideEdid()
: m_bSaved(false)
{
}

//-----------------------------------------------------------------------------
//! \brief Destrutor : Restores EDID to original settings
TegraDispTestComn::OverrideEdid::~OverrideEdid()
{
    if (m_bSaved)
    {
        bool isNull = true;
        for (UINT32 i=0; i < m_Edid.size(); i++)
        {
            if (m_Edid[i] != 0)
            {
                isNull = false;
                break;
            }
        }
        if (isNull)
        {
            m_pDisplay->SetEdid(m_Display, nullptr, 0);
        }
        else
        {
            m_pDisplay->SetEdid(m_Display, &m_Edid[0],
                    static_cast<UINT32>(m_Edid.size()));
        }
    }
}

//-----------------------------------------------------------------------------
//! \brief Override the EDID
//!
//! \param pDisplay : Pointer to display to override
//! \param display  : Display ID to override
//! \param edid     : New EDID to use
//!
//! \return OK if successful, not OK otherwise
RC TegraDispTestComn::OverrideEdid::SetOverride
(
    TegraDisplay*              pDisplay,
    UINT32                     display,
    const vector<UINT08>&      edid
)
{
    MASSERT(!m_bSaved);
    MASSERT(pDisplay);
    MASSERT(display);
    MASSERT(!edid.empty());
    m_pDisplay = pDisplay;
    m_Display  = display;
    RC rc;
    if (m_pDisplay->IsEdidFaked(display))
    {
        CHECK_RC(m_pDisplay->GetEdid(display, &m_Edid, true));
    }
    CHECK_RC(m_pDisplay->SetEdid(display, &edid[0],
                static_cast<UINT32>(edid.size())));
    m_bSaved = true;
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Constructor : set manual update mode in the display
TegraDispTestComn::ManualDisplayUpdateMode::ManualDisplayUpdateMode
(
    Display *pDisplay
)
: m_pDisplay(pDisplay)
 ,m_SavedUpdateMode(Display::AutoUpdate)
{
    if (m_pDisplay)
    {
        m_SavedUpdateMode = m_pDisplay->GetUpdateMode();
        m_pDisplay->SetUpdateMode(Display::ManualUpdate);
    }
}

//-----------------------------------------------------------------------------
//! \brief Destrutor : Restores manual update mode to original settings
TegraDispTestComn::ManualDisplayUpdateMode::~ManualDisplayUpdateMode()
{
    if (m_pDisplay)
    {
        m_pDisplay->SetUpdateMode(m_SavedUpdateMode);
    }
}

JS_CLASS_VIRTUAL_INHERIT(TegraDispTestComn, GpuTest,
                         "CheetAh display test common code.");

PROP_CONST(TegraDispTestComn, mCommand, 1);
PROP_CONST(TegraDispTestComn, mNonBurstSPulse, 2);
PROP_CONST(TegraDispTestComn, mNonBurstSEvent, 3);
PROP_CONST(TegraDispTestComn, mBurst, 4);

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(TegraDispTestComn,
                  SetModes,
                  2,
                  "Set the modes cheetah display test will test, parameter 2 "
                  "is hdmi, dsi, edp, lcd")
{
    JavaScriptPtr pJs;

    // Check the arguments.
    string connection;
    JsArray modes;
    if
    (
            (NumArguments != 2)
            || (OK != pJs->FromJsval(pArguments[0], &connection))
            || (OK != pJs->FromJsval(pArguments[1], &modes))
    )
    {
        JS_ReportError(pContext,
                     "Usage: TegraDispTestComn.SetModes(connection, [modes])");
        return JS_FALSE;
    }

    TegraDispTestComn * pTegraDispComn;
    if ((pTegraDispComn = JS_GET_PRIVATE(TegraDispTestComn, pContext, pObject,
                                         NULL)) != 0)
    {
        for (UINT32 i = 0; i < modes.size(); i++)
        {
            JsArray jsa;
            TegraDispTestComn::TestMode testMode;
            JSObject * pJsDsiInfo;
            JSObject * pJsDpInfo;
            JsArray edid;
            JsArray jsDsiInfoBlock;
            UINT32 opeFmt;
            UINT32 opeYuvBpp;

            if (
               (OK != pJs->FromJsval(modes[i], &jsa)) ||
               (jsa.size() != 10) ||
               (OK != pJs->FromJsval(jsa[0], &testMode.mode.width)) ||
               (OK != pJs->FromJsval(jsa[1], &testMode.mode.height)) ||
               (OK != pJs->FromJsval(jsa[2], &testMode.mode.depth)) ||
               (OK != pJs->FromJsval(jsa[3], &testMode.mode.refreshRate)) ||
               (OK != pJs->FromJsval(jsa[4], &testMode.mode.bInterlaced)) ||
               (OK != pJs->FromJsval(jsa[5], &pJsDsiInfo)) ||
               (OK != pJs->FromJsval(jsa[6], &pJsDpInfo)) ||
               (OK != pJs->FromJsval(jsa[7], &edid)) ||
               (OK != pJs->FromJsval(jsa[8], &opeFmt)) ||
               (OK != pJs->FromJsval(jsa[9], &opeYuvBpp))
               )
            {
                JS_ReportError(pContext,
                               "Usage: %s.SetModes(connection, [modes])",
                               pTegraDispComn->GetName().c_str());
                return JS_FALSE;
            }
            if (opeFmt > static_cast<UINT32>(Display::OpeFmt::MAX))
            {
                pJs->Throw(pContext, RC::ILWALID_ARGUMENT,  "Invalid Output Pixel encoding format");
                return JS_FALSE;
            }
            testMode.mode.opeFormat = static_cast<Display::OpeFmt>(opeFmt);

            if (opeYuvBpp > static_cast<UINT32>(Display::OpeYuvBpp::MAX))
            {
                pJs->Throw(pContext, RC::ILWALID_ARGUMENT,  "Invalid BPP for OPE format");
                return JS_FALSE;
            }
            testMode.mode.opeYuvBpp = static_cast<Display::OpeYuvBpp>(opeYuvBpp);

            testMode.mode.edid.resize(edid.size());
            for (UINT32 j=0; j < edid.size(); j++)
            {
                if (OK != pJs->FromJsval(edid[j],
                            &testMode.mode.edid[j]))
                {
                    JS_ReportError(pContext, "Invalid EDID");
                    return JS_FALSE;
                }
            }
            if ("hdmi" == connection)
            {
                testMode.connection = TegraDisplay::CT_HDMI;
            }
            else if ("dsi" == connection)
            {
                testMode.connection = TegraDisplay::CT_DSI;

                if ((OK != pJs->GetProperty(pJsDsiInfo, "lanes",
                                            &testMode.mode.dsiInfo.lanes)) ||
                    (OK != pJs->GetProperty(pJsDsiInfo, "pix_fmt",
                                            &testMode.mode.dsiInfo.pixFmt)) ||
                    (OK != pJs->GetProperty(pJsDsiInfo, "ganged",
                                            &testMode.mode.dsiInfo.bGanged)) ||
                    (OK != pJs->GetProperty(pJsDsiInfo, "info_block",
                                            &jsDsiInfoBlock)))
                {
                    JS_ReportError(pContext, "Missing DSI info");
                    return JS_FALSE;
                }

                testMode.mode.dsiInfo.infoBlock.resize(jsDsiInfoBlock.size());
                for (UINT32 j=0; j < jsDsiInfoBlock.size(); j++)
                {
                    if (OK != pJs->FromJsval(jsDsiInfoBlock[j],
                                &testMode.mode.dsiInfo.infoBlock[j]))
                    {
                        JS_ReportError(pContext, "Invalid DSI info block");
                        return JS_FALSE;
                    }
                }
                if (testMode.mode.dsiInfo.infoBlock.empty())
                {
                    JS_ReportError(pContext, "Missing DSI info block");
                    return JS_FALSE;
                }
                if (testMode.mode.edid.empty())
                {
                    JS_ReportError(pContext, "Missing EDID");
                    return JS_FALSE;
                }
            }
            else if ("lcd" == connection)
            {
                testMode.connection = TegraDisplay::CT_LCD;
            }
            else if ("edp" == connection)
            {
                testMode.connection = TegraDisplay::CT_EDP;

                if ((OK != pJs->GetProperty(pJsDpInfo, "lanes",
                                            &testMode.mode.dpInfo.lanes)) ||
                    (OK != pJs->GetProperty(pJsDpInfo, "linkspeed",
                                            &testMode.mode.dpInfo.linkSpeed)))
                {
                    JS_ReportError(pContext, "Missing DP info");
                    return JS_FALSE;
                }
            }
            else
            {
                JS_ReportError(pContext, "Invalid display type, only 'lcd', "
                                         "'edp', 'hdmi' or 'dsi' accepted");
                return JS_FALSE;
            }
            pTegraDispComn->AddMode(testMode);
        }

        return JS_TRUE;
    }

    return JS_FALSE;
}

CLASS_PROP_READWRITE(TegraDispTestComn, UseRealDisplays, bool,
    "Test the built in displays only (and for panels only the native resolution");
