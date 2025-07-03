/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//!
//! \file modesetlib.h
//!       Modeset Library calls

#ifndef INCLUDED_MODESETLIB_H
#define INCLUDED_MODESETLIB_H

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"         // For CLASS_PROP_READWRITE
#include "core/include/jscript.h"        // For JavaScript linkage
#include "gpu/display/lwdisplay/lw_disp.h"
#include "gpu/display/lwdisplay/lw_disp_utils.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "hwref/disp/v03_00/dev_disp.h" // LW_PDISP_FE_DEBUG_CTL
#include "lwmisc.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/tests/rm/utility/hdcputils.h"
#include "gpu/display/modeset_utils.h"
#include "gpu/display/modeset_utils.h"
#include "gpu/display/evo_dp.h"
#include "class/clc37d.h" // LWC37D_CORE_CHANNEL_DMA
#include "gpu/display/dpmgr.h"

using namespace std;

namespace Modesetlib
{

#define DEFAULT_PANEL_VALUE 0xff
typedef UINT32 Head;
typedef UINT32 DisplayId;
 
    enum DisplayMode
    {
        ENABLE_BYPASS_SETTINGS,
        ENABLE_SDR_SETTINGS,
        ENABLE_HDR_SETTINGS,
    };

    struct WindowSettings
    {
        LwDispWindowSettings winSettings;
        // In case of semiplanar surfSettings will be used for Y Surface
        LwDispSurfaceParams  surfSettings; 
        LwDispSurfaceParams  uvSurfSettings;
        UINT32 headNum;
        UINT32 windowNum;
        EotfLutGenerator::EotfLwrves inputLutLwrve;
        EetfLutGenerator::EetfLwrves tmoLutLwrve;
    };

    struct EnumeratedSet
    {
        DisplayID displayId;
        bool fakeDisplay;
        EnumeratedSet(DisplayID dispID, bool bfakeDisplay)
        {
            displayId = dispID;
            fakeDisplay = bfakeDisplay;
        }
    };

    struct DisplayResolution
    {
        UINT32 width;
        UINT32 height;
        UINT32 refreshrate;
        UINT32 depth;
        UINT32 pixelClockHz;
        DisplayResolution()
        {
            pixelClockHz = width = height = refreshrate = depth = DEFAULT_PANEL_VALUE;
        }
        void Reset()
        {
            pixelClockHz = width = height = refreshrate = depth = DEFAULT_PANEL_VALUE;
        }
    };

    struct WindowParam
    {
        UINT32 windowNum;
        UINT32 width;
        UINT32 height;
        UINT32 headNum; // To which head it should be connected
        DisplayMode mode;
        EotfLutGenerator::EotfLwrves inputLutLwrve;
        EetfLutGenerator::EetfLwrves tmoLutLwrve;

        WindowParam()
        {
            windowNum = 0;
            width = 0;
            height = 0;
            headNum = 0;
            mode = ENABLE_BYPASS_SETTINGS;
            inputLutLwrve = EotfLutGenerator::EOTF_LWRVE_NONE;
            tmoLutLwrve = EetfLutGenerator::EETF_LWRVE_NONE;
        }
    };

    enum ColorGamut
    {
        REC_709,
        DCI_P3,
        BT2020,
        D60DCI,
        ADOBE98,
    };

    struct Chromaticities
    {
        float red_x, red_y;
        float green_x, green_y;
        float blue_x, blue_y;
        float wp_x, wp_y;
    };

    const Chromaticities chromacityList[] =
    {
       { 0.64000f, 0.33000f, 0.30000f, 0.60000f, 0.15000f, 0.06000f, 0.31270f, 0.32900f }, // rec709
       { 0.68000f, 0.32000f, 0.26500f, 0.69000f, 0.15000f, 0.06000f, 0.31400f, 0.35100f }, // DCI-P3
       { 0.70800f, 0.29200f, 0.17000f, 0.79700f, 0.13100f, 0.04600f, 0.31270f, 0.32900f }, // bt2020
       { 0.68000f, 0.32000f, 0.26500f, 0.69000f, 0.15000f, 0.06000f, 0.32168f, 0.33767f }, // D60 DCI
       { 0.64000f, 0.33000f, 0.21000f, 0.71000f, 0.15000f, 0.06000f, 0.31270f, 0.32900f }, // Adobe98
    };


    typedef vector<WindowParam> WindowParams;

    class DisplayPanel;

    class Modeset
    {
    protected:
        DisplayPanel *m_pDisplayPanel;
        Display *m_pDisplay;
        LwDisplay *m_pLwDisplay;
        LwDispCoreChnContext *m_pCoreChCtx;
        LwDispPanelMgr::LwDispPanelType m_panelType;
        LwDispCRCSettings m_crcSettings;

    public:
        ColorGamut  m_color;
        bool        m_modesetDone;
        bool        m_semiPlanar;
        Modeset(DisplayPanel *pDisplayPanel, Display *pDisplay);
        RC AllocateWindowsAndSurface();
        RC FreeWindowsAndSurface();
        RC VerifyDisp(bool manualVerif, string crcFileName, string goldenFileDir, bool generateGold);
        RC GetListedModes(vector<Display::Mode> *pListedModes);
        RC SelectUserInputs(DisplayPanel *pDisplayPanel, bool &bExit);
        RC SetupChannelImage();
        RC SetWindowImage(DisplayPanel *pDisplayPanel);
        RC AutoVerification(bool generateGold);
        RC AutoVerification(string crcFileName,
                            string goldenFolderDir,
                            bool generateGold);
        virtual RC Initialize() = 0;
        virtual RC SetMode() = 0;
        virtual RC Detach() = 0;
        virtual LwDispPanelMgr::LwDispPanelType GetPanelType()
        {
            return m_panelType;
        }

        RC SetLwstomSettings();
        RC ClearLwstomSettings();
        RC VerifyHDCP();

        RC SetVRRSettings(bool enable, bool bLegacy = false);
        RC EnableVRR(DisplayPanel *pDisplayPanel, bool bLegacy = false);
        RC DisableVRR(DisplayPanel *pDisplayPanel);
        RC ForceDSCBitsPerPixel(UINT32 forceDscBitsPerPixelX16);
        RC ForceYUV(Display::ColorSpace);
        RC SendInterlockedUpdates(DisplayPanel *pDisplayPanel);
        RC AllocateWindowsLwstomWinSettings(DisplayPanel *pDisplayPanel);
        RC SetupLwrsorChannelImage();
        RC AllocateLwrsorImm();
        bool IsPanelTypeDP();
        bool IsPanelTypeHDMIFRL();
        UINT32 getMaxHeads() {return m_MaxHeads;}
        UINT32 getMaxWindows() {return m_MaxWindows;}
        UINT32 getMaxSors() {return m_MaxSors;}
        DisplayMode getDispMode() {return m_mode;}
        void setDispMode(DisplayMode mode){ m_mode = mode;}
        bool IsHdrCapable() {return m_IsHdrCapable;}
        RC UpdateWindowsAndHeadSettings();
        UINT32 Disp_reg_dump(UINT32 offset);

        virtual ~Modeset()
        {
            //
            // Detach Display if the Display is attached
            // Clear and free the allocated window surfaces
        }
    private:
        UINT32 m_MaxHeads;
        UINT32 m_MaxWindows;
        UINT32 m_MaxSors;
        DisplayMode m_mode;
        bool m_IsHdrCapable;
        RC UpdateWindowInputLut(LwDispWindowChnContext *pWinChnContext, UINT32 index);
        RC UpdateWindowTmoLut(LwDispWindowChnContext *pWinChnContext, UINT32 index);
        RC UpdateHeadOutputLut();

    };

    class DisplayPanel
    {
    public :
        DisplayID displayId;
        DisplayID secDisplayId;
        UINT32 ConnectorID;
        vector<WindowSettings> windowSettings;
        bool            bValidLwrsorSettings;
        LwDispSurfaceParams     lwrsorSurfParams;
        LwDispLwrsorSettings lwrsorSettings;
        // User Inputted Settings
        WindowParams winParams;
        DisplayResolution resolution;
        string edidfileName;
        string orProtocol;
        bool   isFakeDisplay;
        bool   bActiveModeset;
        bool   bEnableVRR;
        UINT32 sor;
        UINT32 head;
        LwDispORSettings::PixelDepth pixelDepth;
        Modeset *pModeset;
        string m_crcFileName;
        string m_goldenCrcDir;
        OetfLutGenerator::OetfLwrves outputLutLwrve;

        struct
        {
            UINT08 *pEdid;
            UINT32 edidSize;
        } fakeDpProperties;

        WindowSettings *allocWindowSettings()
        {
            windowSettings.resize(windowSettings.size()+1);

            return &windowSettings[windowSettings.size()-1];
        }
        DisplayPanel *pSecondaryPanel;

        DisplayPanel()
        {
            sor = head = DEFAULT_PANEL_VALUE; // DEFAULT means take defaults
            edidfileName = "";
            orProtocol = "";
            isFakeDisplay = false;
            pModeset =  NULL;
            bActiveModeset = false;
            bEnableVRR = false;
            pixelDepth = LwDispORSettings::OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_ILWALID;
            resolution.pixelClockHz = DEFAULT_PANEL_VALUE;
            bValidLwrsorSettings = false;
            pSecondaryPanel = NULL;
            fakeDpProperties.pEdid = NULL;
            fakeDpProperties.edidSize = 0;
            ConnectorID = 0;
            outputLutLwrve = OetfLutGenerator::OETF_LWRVE_NONE;
        }

        void Reset()
        {
            resolution.Reset();
            sor = head =  DEFAULT_PANEL_VALUE; // DEFAULT means take defaults
            edidfileName = "";
            orProtocol = "";
            isFakeDisplay = false;
            pModeset = NULL;
            bActiveModeset = false;
            bEnableVRR = false;
            pixelDepth = LwDispORSettings::OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_ILWALID;
            resolution.pixelClockHz = DEFAULT_PANEL_VALUE;
            bValidLwrsorSettings = false;
            outputLutLwrve = OetfLutGenerator::OETF_LWRVE_NONE;
        }

        bool HDRCapable(Display *m_pDisplay);

        ~DisplayPanel();
    };

    class DisplayPanelHelper
    {

    public:
        DisplayPanelHelper(Display *pDisplay);
        RC EnumerateDisplayPanels
        (
            string protocolFilter,
            vector <DisplayPanel *>&pDisplayPanels,
            vector<pair<DisplayPanel *, DisplayPanel *>> &dualSSTPairs,
            bool enumAllDcbDisplays
        );
        RC EnumerateDisplayPanels
        (
            string protocolFilter,
            vector <DisplayPanel *>&pDisplayPanels,
            vector<pair<DisplayPanel *, DisplayPanel *>> &dualSSTPairs,
            bool enumAllDcbDisplays,
            UINT32 numOfSinks,
            bool isMST,
            UINT08* pEdid,
            UINT32 edidSize
        );
        void BuildAllSingleHeadMultiStreamPairs
        (
            vector <DisplayPanel *>&pDisplayPanels,
            vector<pair<DisplayPanel *, DisplayPanel *>> &dualSSTPairs
        );
        RC ListAndSelectDisplayPanel(UINT32& panelIndex, bool &bExit);
        RC ConfigureModesetPanel
        (
            pair<DisplayPanel *, DisplayPanel *> &dualSSTPair,
            LwDispPanelMgr::LwDispPanelType panelType
        );
        RC ConfigureModesetPanel
        (
            DisplayPanel *pDisplayPanel,
            LwDispPanelMgr::LwDispPanelType panelType
        );
        RC DynamicWindowMove(vector <DisplayPanel *>pDisplayPanels);
        RC AutomatedDynamicWindowMove(vector <DisplayPanel *>pDisplayPanels);
        ~DisplayPanelHelper();

    private:
        Display *m_pDisplay;
        string m_protocolFilter;
        vector<DisplayPanel *>m_pDisplayPanels;
        vector<pair<DisplayPanel *, DisplayPanel *>> m_dualSSTPairs;
        UINT32 m_numOfSinks;
        bool m_isMST;
        UINT08* m_pMSTEdid;
        UINT32 m_mstEdidSize;
    };

    class Modeset_TMDS : public Modeset
    {
    public:
        Modeset_TMDS(DisplayPanel *pDisplayPanel, Display *pDisplay);

        RC ReadWriteI2cRegister
        (
            GpuSubdevice *pSubdev,
            LwU32 I2CPort,
            UINT32 I2CAddress,
            UINT32 I2CRegisterOFfset,
            UINT32 *pReadData,
            bool bReadWrite = false
        );

        virtual RC Initialize();
        virtual RC SetMode();
        virtual RC Detach();
        virtual ~Modeset_TMDS();
    };

    class Modeset_DP : public Modeset
    {
    public:

        DisplayPanel *m_pDpDispPanel;
        DisplayPortMgr::DP_LINK_CONFIG_DATA m_dpLinkConfig;
        pair<DisplayPanel *, DisplayPanel *> m_dualSSTPair;

        Modeset_DP(DisplayPanel *pDisplayPanel, Display *pDisplay);
        Modeset_DP
        (
            pair<DisplayPanel *, DisplayPanel *> &dualSSTPair,
            Display *pDisplay
        ); // For Dual SST
        virtual RC Initialize();
        virtual RC SetMode();
        virtual RC Detach();
        virtual ~Modeset_DP();
        virtual RC DoDpLinkTraining();
        virtual RC SetupLinkConfig(DisplayPortMgr::DP_LINK_CONFIG_DATA dpLinkConfig);
        RC SendSDP(Head head, DisplayId displayId);
    };

    class Modeset_HDMI : public Modeset_TMDS
    {
    public:
        bool m_SupportsHdmi2;
        Modeset_HDMI(DisplayPanel *pDisplayPanel, Display *pDisplay);
        virtual RC Initialize();
        virtual RC SetMode();
        virtual RC Detach();
    };

    class Modeset_HDMI_FRL : public Modeset_TMDS
    {
    public:
        struct HDMI_FRL_LINK_CONFIG_DATA
        {
            UINT32 linkRate;
            UINT32 laneCount;
            bool   bDsc12;
        };
        HDMI_FRL_LINK_CONFIG_DATA m_frlLinkConfig;
        bool m_SupportsHdmiFrl;
        Modeset_HDMI_FRL(DisplayPanel *pDisplayPanel, Display *pDisplay);
        virtual RC Initialize();
        virtual RC SetMode();
        virtual RC Detach();
        virtual RC SetupLinkConfig(HDMI_FRL_LINK_CONFIG_DATA frlLinkConfig);
        virtual RC DoFrlLinkTraining();
    };

    class Modeset_DSI : public Modeset
    {
    public:
        Modeset_DSI(DisplayPanel *pDisplayPanel, Display *pDisplay);

        virtual RC Initialize();
        virtual RC SetMode();
        virtual RC Detach();
        virtual ~Modeset_DSI();
    };
}
#endif //INCLUDED_MODESETLIB_H
