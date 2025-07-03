/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//!
//! \file Modeset library implementation
//!

#include "modesetlib.h"

#include "core/include/modsedid.h"
#include "core/include/simpleui.h"
#include "ctrl/ctrl0073.h"
#include "ctrl/ctrl0073/ctrl0073dfp.h"
#include "ctrl/ctrl5070/ctrl5070chnc.h" // IsModePossible
#include "class/clc570.h"
#include "device/interface/i2c.h"
#include "dpsdp.h"
#include "gpu/disp/hdmi_spec_addendum.h"

#if defined(TEGRA_MODS)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#endif

using namespace std;
using namespace Modesetlib;

DisplayPanelHelper::DisplayPanelHelper(Display *pDisplay)
:m_pDisplay(pDisplay)
{
    m_numOfSinks = 0;
    m_isMST = false;
    m_pMSTEdid = NULL;
    m_mstEdidSize = 0;
}

RC DisplayPanelHelper::EnumerateDisplayPanels
(
    string protocolFilter,
    vector <DisplayPanel *>&pDisplayPanels,
    vector<pair<DisplayPanel *, DisplayPanel *>> &dualSSTPairs,
    bool enumAllDcbDisplays,
    UINT32 numOfSinks,
    bool isMST,
    UINT08* pEdid,
    UINT32 edidSize
)
{
    RC rc;
    m_numOfSinks = numOfSinks;
    m_isMST = isMST;

    if(m_isMST && enumAllDcbDisplays)
    {
        if(!pEdid || !edidSize)
        {
            Printf(Tee::PriHigh, "%s: No Custom Edid set for MST display. Will use default Edid\n", __FUNCTION__);
        }
        else
        {
            m_pMSTEdid = (UINT08 *)malloc(edidSize * sizeof(UINT08));
            memcpy(m_pMSTEdid, pEdid, edidSize);
            m_mstEdidSize = edidSize;
        }
    }

    CHECK_RC(EnumerateDisplayPanels(protocolFilter, pDisplayPanels, dualSSTPairs,enumAllDcbDisplays));
    return rc;
}

RC DisplayPanelHelper::EnumerateDisplayPanels
(
    string protocolFilter,
    vector <DisplayPanel *>&pDisplayPanels,
    vector<pair<DisplayPanel *, DisplayPanel *>> &dualSSTPairs,
    bool enumAllDcbDisplays
)
{
    RC rc;

    DisplayIDs supportedDids;
    DisplayIDs detectedDids;
    DisplayIDs fakeDids;
    vector<EnumeratedSet>enumedSet;

    m_protocolFilter = protocolFilter;

    CHECK_RC(m_pDisplay->GetDetected(&detectedDids));
    Printf(Tee::PriHigh, "All Detected displays = \n");
    m_pDisplay->PrintDisplayIds(Tee::PriHigh, detectedDids);

    // Detected part
    for (UINT32 i=0; i < detectedDids.size(); i++)
    {
        if(DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay,
                                                  detectedDids[i],
                                                  protocolFilter))
        {
            enumedSet.push_back(EnumeratedSet(detectedDids[i], false));
        }
    }

    // Fake Displays
    if(enumAllDcbDisplays)
    {
        //get all the available displays
        DisplayIDs supportedDids;
        CHECK_RC(m_pDisplay->GetConnectors(&supportedDids));

        DisplayIDs displaysToFake;
        for(UINT32 i = 0; i < supportedDids.size(); i++)
        {
            if (!m_pDisplay->IsDispAvailInList(supportedDids[i], detectedDids) &&
                DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay, supportedDids[i], protocolFilter))
            {
                fakeDids.push_back(supportedDids[i]);
            }
        }

        if(m_isMST && (fakeDids.size() > 0))
        {
            //For MST, first devices have to be simulated to get the
            //dynamic Dids
            UINT32  simulatedDisplayIDsMask = 0;
            DisplayIDs simulatedDids;
            CHECK_RC(m_pDisplay->EnableSimulatedDPDevices(
                fakeDids[0].Get(), 0, Display::DPMultistreamEnabled,
                m_numOfSinks, m_pMSTEdid, m_mstEdidSize, &simulatedDisplayIDsMask));
            m_pDisplay->DispMaskToDispID(simulatedDisplayIDsMask, simulatedDids);

            for(UINT32 i = 0; i < simulatedDids.size(); i++)
            {
                enumedSet.push_back(EnumeratedSet(simulatedDids[i], true));
            }
        }
        else
        {
            for(UINT32 j = 0; j < fakeDids.size(); j++)
            {
                enumedSet.push_back(EnumeratedSet(fakeDids[j], true));
            }
        }
    }

    string orProtocol= "";
    pDisplayPanels.resize(enumedSet.size());

    for (UINT32 i = 0; i < enumedSet.size(); i++)
    {
       pDisplayPanels[i] = new DisplayPanel();
       CHECK_RC_MSG(m_pDisplay->GetProtocolForDisplayId(enumedSet[i].displayId, orProtocol), "No associated OR protocol");
       pDisplayPanels[i]->orProtocol = orProtocol;
       pDisplayPanels[i]->displayId = enumedSet[i].displayId;
       pDisplayPanels[i]->isFakeDisplay = enumedSet[i].fakeDisplay;
       if(m_pDisplay->IsMultiStreamDisplay(pDisplayPanels[i]->displayId.Get()))
       {
            UINT32 connID;
            m_pDisplay->GetDisplayPortMgr()->GetConnectorID(pDisplayPanels[i]->displayId.Get(), &connID);
            pDisplayPanels[i]->ConnectorID = connID;
       }
       else
       {
            pDisplayPanels[i]->ConnectorID = pDisplayPanels[i]->displayId.Get();
       }

       m_pDisplayPanels.push_back(pDisplayPanels[i]);
    }

    vector <DisplayPanel *>pDpDisplayPanels;

    for (UINT32 i=0; i < pDisplayPanels.size(); i++)
    {
        if (pDisplayPanels[i]->orProtocol.find("DP") != string::npos)
        {
            CHECK_RC(ConfigureModesetPanel(pDisplayPanels[i],
                LwDispPanelMgr::LWD_PANEL_TYPE_DP));
            pDpDisplayPanels.push_back(pDisplayPanels[i]);
        }
        else if (pDisplayPanels[i]->orProtocol.find("DSI") != string::npos)
        {
            CHECK_RC(ConfigureModesetPanel(pDisplayPanels[i],
                LwDispPanelMgr::LWD_PANEL_TYPE_DSI));
        }
        else if(pDisplayPanels[i]->orProtocol.find("TMDS") != string::npos)
        {
            bool bSupportsHdmi = false;
            bool bSupportsHdmiFrl = false;
            RC rcHdmi = m_pDisplay->GetHdmiSupport(pDisplayPanels[i]->displayId, &bSupportsHdmi);
            RC rcHdmiFrl = m_pDisplay->GetHdmiFrlSupport(pDisplayPanels[i]->displayId, &bSupportsHdmiFrl);
            if (rcHdmiFrl == OK && bSupportsHdmiFrl)
            {
                CHECK_RC(ConfigureModesetPanel(pDisplayPanels[i],
                    LwDispPanelMgr::LWD_PANEL_TYPE_HDMI_FRL));
            }
            else if (rcHdmi == OK && bSupportsHdmi)
            {
                CHECK_RC(ConfigureModesetPanel(pDisplayPanels[i],
                    LwDispPanelMgr::LWD_PANEL_TYPE_HDMI));
            }
            else
            {
                CHECK_RC(ConfigureModesetPanel(pDisplayPanels[i],
                    LwDispPanelMgr::LWD_PANEL_TYPE_DVI));
            }
        }
    }

    if (pDpDisplayPanels.size() > 1)
    {
        BuildAllSingleHeadMultiStreamPairs(pDpDisplayPanels, dualSSTPairs);

        for (UINT32 i=0; i < dualSSTPairs.size(); i++)
        {
            CHECK_RC(ConfigureModesetPanel(dualSSTPairs[i],
                LwDispPanelMgr::LWD_PANEL_TYPE_DP_DUAL_SST));
        }
        m_dualSSTPairs = dualSSTPairs;
    }

    return rc;
}

void DisplayPanelHelper::BuildAllSingleHeadMultiStreamPairs
(
    vector <DisplayPanel *>&pDisplayPanels,
    vector<pair<DisplayPanel *, DisplayPanel *>> &dualSSTPairs
)
{
    for (UINT32 i = 0; i < pDisplayPanels.size(); i++)
    {
        for(UINT32 j = i + 1; j < pDisplayPanels.size(); j++)
        {
            if (i != j)
            {
                bool priMST = m_pDisplay->IsMultiStreamDisplay(pDisplayPanels[i]->displayId.Get());
                bool secMST = m_pDisplay->IsMultiStreamDisplay(pDisplayPanels[j]->displayId.Get());

                if(priMST ^ secMST)
                    continue;

                DisplayPanel *pPriDispPanel = new DisplayPanel();
                DisplayPanel *pSecDispPanel = new DisplayPanel();

                pPriDispPanel->orProtocol =  pDisplayPanels[i]->orProtocol;
                pPriDispPanel->displayId = pDisplayPanels[i]->displayId;
                pPriDispPanel->isFakeDisplay = pDisplayPanels[i]->isFakeDisplay;
                pPriDispPanel->ConnectorID = pDisplayPanels[i]->ConnectorID;

                pSecDispPanel->orProtocol =  pDisplayPanels[j]->orProtocol;
                pSecDispPanel->displayId = pDisplayPanels[j]->displayId;
                pSecDispPanel->isFakeDisplay = pDisplayPanels[j]->isFakeDisplay;
                pSecDispPanel->ConnectorID = pDisplayPanels[j]->ConnectorID;

                pPriDispPanel->pSecondaryPanel = pSecDispPanel;

                dualSSTPairs.push_back(make_pair(pPriDispPanel, pSecDispPanel));
            }
        }
    }
}

RC DisplayPanelHelper::ConfigureModesetPanel
(
    DisplayPanel *pDisplayPanel,
    LwDispPanelMgr::LwDispPanelType panelType
)
{
    RC rc;
    switch (panelType)
    {
        case LwDispPanelMgr::LWD_PANEL_TYPE_DP:
        case LwDispPanelMgr::LWD_PANEL_TYPE_DP_MST:
        case LwDispPanelMgr::LWD_PANEL_TYPE_EDP:
            pDisplayPanel->pModeset = new Modeset_DP(pDisplayPanel, m_pDisplay);
            break;
        case LwDispPanelMgr::LWD_PANEL_TYPE_DVI:
            pDisplayPanel->pModeset = new Modeset_TMDS(pDisplayPanel, m_pDisplay);
            break;
        case LwDispPanelMgr::LWD_PANEL_TYPE_HDMI:
            pDisplayPanel->pModeset = new Modeset_HDMI(pDisplayPanel, m_pDisplay);
            break;
        case LwDispPanelMgr::LWD_PANEL_TYPE_HDMI_FRL:
            pDisplayPanel->pModeset = new Modeset_HDMI_FRL(pDisplayPanel, m_pDisplay);
            break;
        case LwDispPanelMgr::LWD_PANEL_TYPE_DSI:
            pDisplayPanel->pModeset = new Modeset_DSI(pDisplayPanel, m_pDisplay);
            break;
        case LwDispPanelMgr::LWD_PANEL_TYPE_NONE:
        default:
            MASSERT(!"Unrecognized Panel");
            return RC::SOFTWARE_ERROR;
    }
    return rc;
}

RC DisplayPanelHelper::ConfigureModesetPanel
(
    pair<DisplayPanel *, DisplayPanel *> &dualSSTPair,
    LwDispPanelMgr::LwDispPanelType panelType
)
{
    RC rc;
    switch (panelType)
    {
        case LwDispPanelMgr::LWD_PANEL_TYPE_DP_DUAL_SST:
        case LwDispPanelMgr::LWD_PANEL_TYPE_DP_DUAL_MST:
            (dualSSTPair.first)->pModeset = new Modeset_DP(dualSSTPair, m_pDisplay);
            break;
        default:
            MASSERT(!"Unrecognized Panel");
            return RC::SOFTWARE_ERROR;
    }
    return rc;
}

RC DisplayPanelHelper::ListAndSelectDisplayPanel(UINT32& panelIndex, bool &bExit)
{
    RC rc;
    Printf(Tee::PriHigh, "\n #########################DISPLAY PANELS INFORMATION######################### \n");

    UINT32 numPanels =  static_cast<UINT32>(m_pDisplayPanels.size());

    if (!numPanels)
    {
        return RC::SOFTWARE_ERROR;
    }
    UINT32 i= 0;
    for (i = 0; i < numPanels; i++)
    {
        Printf(Tee::PriHigh, "\n INDEX: %d", i);
        Printf(Tee::PriHigh, "\n           DisplayID    : 0x%X", (UINT32)m_pDisplayPanels[i]->displayId);
        Printf(Tee::PriHigh, "\n           OR Protocol  : %s", m_pDisplayPanels[i]->orProtocol.c_str());
        Printf(Tee::PriHigh, "\n           Fake Display : %d", m_pDisplayPanels[i]->isFakeDisplay);
        if (m_pDisplayPanels[i]->pModeset)
        {
            if (m_pDisplayPanels[i]->pModeset->GetPanelType() == LwDispPanelMgr::LWD_PANEL_TYPE_HDMI)
                Printf(Tee::PriHigh, "\n            Connector : HDMI");
            else if (m_pDisplayPanels[i]->pModeset->GetPanelType() ==
                                            LwDispPanelMgr::LWD_PANEL_TYPE_DSI)
                Printf(Tee::PriHigh, "\n            Connector : DSI");
            else if (m_pDisplayPanels[i]->pModeset->GetPanelType() == LwDispPanelMgr::LWD_PANEL_TYPE_DVI)
                Printf(Tee::PriHigh, "\n            Connector : DVI");
            else if (m_pDisplayPanels[i]->pModeset->GetPanelType() == LwDispPanelMgr::LWD_PANEL_TYPE_DP ||
                m_pDisplayPanels[i]->pModeset->GetPanelType() == LwDispPanelMgr::LWD_PANEL_TYPE_DP_MST)
            {
                if(m_pDisplay->IsMultiStreamDisplay(m_pDisplayPanels[i]->displayId.Get()))
                {
                    Printf(Tee::PriHigh, "\n            Connector : DP-MST");
                }
                else
                {
                    Printf(Tee::PriHigh, "\n            Connector : DP-SST");
                }
                
            }
        }
        Printf(Tee::PriHigh, "\n ============================= \n");
    }

    UINT32 totalIndexs = numPanels + (UINT32) m_dualSSTPairs.size();
    for (UINT32 j = 0; j < (UINT32) m_dualSSTPairs.size(); j++)
    {
        Printf(Tee::PriHigh, "\n INDEX: %d", numPanels + j);
        Printf(Tee::PriHigh, "\n           DisplayID    : 0x%X, DisplayID    : 0x%X ",
            (UINT32)(m_dualSSTPairs[j].first)->displayId,
            (UINT32)(m_dualSSTPairs[j].second)->displayId);
        if(m_pDisplay->IsMultiStreamDisplay((m_dualSSTPairs[j].first)->displayId.Get()))
        {
            Printf(Tee::PriHigh, "\n           PANEL TYPE  : DUAL_MST");
        }
        else
        {
            Printf(Tee::PriHigh, "\n           PANEL TYPE  : DUAL_SST");
        }
        Printf(Tee::PriHigh, "\n           Fake Display  panel 1: %d, panel 2: %d",
            (m_dualSSTPairs[j].first)->isFakeDisplay,
            (m_dualSSTPairs[j].second)->isFakeDisplay);

        Printf(Tee::PriHigh, "\n ============================= \n");
    }

    SimpleUserInterface * pInterface = new SimpleUserInterface(true);

    Printf(Tee::PriHigh,
        "\n > ######################## DISPLAY PANEL INFO END ################################ \n");

    Printf(Tee::PriHigh,
        "\n SELECT  DISPLAY PANEL  INDEX TO RUN or EXIT To EXIT\n");

    string inputNumStr = pInterface->GetLine();
    if (inputNumStr.find("EXIT") != string::npos)
    {
        bExit = true;
    }
    else
    {
        panelIndex  = atoi(inputNumStr.c_str());
        if (!(panelIndex < totalIndexs))
        {
            Printf(Tee::PriHigh,
                "\n %s: Wrong panel index input\n", __FUNCTION__);
            rc = RC::SOFTWARE_ERROR;
        }

        Printf(Tee::PriHigh,
            "\n Selected Panel Index %d \n", panelIndex);
    }

    delete pInterface;

    return rc;
}

DisplayPanelHelper:: ~DisplayPanelHelper()
{
    bool bDpSimulation = false;
    for (UINT32 i=0; i < m_pDisplayPanels.size(); i++)
    {
        if (m_pDisplayPanels[i]->isFakeDisplay && (m_pDisplayPanels[i]->pModeset) &&
            (m_pDisplayPanels[i]->pModeset->GetPanelType() == LwDispPanelMgr::LWD_PANEL_TYPE_DP))
            {
                bDpSimulation = true;
            }
        delete m_pDisplayPanels[i];
    }

    for (UINT32 i= 0; i < m_dualSSTPairs.size(); i++)
    {
        delete m_dualSSTPairs[i].second;
        delete m_dualSSTPairs[i].first;
    }

    if  (bDpSimulation)
    {
        // This will disable all simulated DP devices
        m_pDisplay->DisableAllSimulatedDPDevices(Display::SkipRealDisplayDetection, nullptr);
    }

    m_dualSSTPairs.clear();
    m_pDisplayPanels.clear();

    if(m_pMSTEdid != NULL)
    {
        free(m_pMSTEdid);
    }
}

RC DisplayPanelHelper::AutomatedDynamicWindowMove(vector <DisplayPanel *>pDisplayPanels)
{
    RC rc;
    DisplayPanel *sourcePanel, *destPanel;
    LwDisplay *pLwDisplay = m_pDisplay->GetLwDisplay();

    if (pDisplayPanels[0]->winParams.size() >= 2)
    {
        sourcePanel = pDisplayPanels[0];
        destPanel= pDisplayPanels[1];
    }
    else
    {
        sourcePanel = pDisplayPanels[1];
        destPanel= pDisplayPanels[0];
    }

    CHECK_RC(pLwDisplay->SetWindowsOwnerDynamic(
            sourcePanel->head,
            1 << (sourcePanel->winParams[sourcePanel->winParams.size() - 1].windowNum),
            LwDisplay::WIN_OWNER_MODE_DETACH,
            LwDisplay::SEND_UPDATE,
            LwDisplay::WAIT_FOR_NOTIFIER));

    CHECK_RC(pLwDisplay->SetWindowsOwnerDynamic(
            destPanel->head,
            1 << (sourcePanel->winParams[sourcePanel->winParams.size() - 1].windowNum),
            LwDisplay::WIN_OWNER_MODE_ATTACH,
            LwDisplay::SEND_UPDATE,
            LwDisplay::WAIT_FOR_NOTIFIER));

    destPanel->winParams.push_back(sourcePanel->winParams[sourcePanel->winParams.size() - 1]);
    destPanel->windowSettings.push_back(sourcePanel->windowSettings[sourcePanel->winParams.size() - 1]);
    sourcePanel->winParams.erase(sourcePanel->winParams.begin() + 1);
    sourcePanel->windowSettings.erase(sourcePanel->windowSettings.begin() + 1);

    return rc;
}

RC DisplayPanelHelper::DynamicWindowMove(vector <DisplayPanel *>pDisplayPanels)
{
    RC rc;
    DisplayPanel *sourcePanel = NULL;
    DisplayPanel *destPanel   = NULL;
    UINT32 sPanelWinNum;
    LwDisplay *pLwDisplay = m_pDisplay->GetLwDisplay();
    SimpleUserInterface * pInterface = new SimpleUserInterface(true);

    do 
    {
        // Check if we have minimum 2 panels for changing the windows dynamically.
        if (pDisplayPanels.size() < 2)
            return rc;

        for (UINT32 i = 0; i < pDisplayPanels.size(); i++)
        {
            Printf(Tee::PriHigh,
                "\n Windows attached to Panel %d\n", i);
            for (auto &winParam: pDisplayPanels[i]->winParams)
            {
                Printf(Tee::PriHigh,
                "%d\t", winParam.windowNum);
            }
            Printf(Tee::PriHigh,"\n");
        }

        Printf(Tee::PriHigh,"\n Choose the Source Panel: \n");
        string inputNumStr = pInterface->GetLine();
        if ((UINT32) atoi(inputNumStr.c_str()) < pDisplayPanels.size())
        {
            sourcePanel = pDisplayPanels[atoi(inputNumStr.c_str())];
        }
        Printf(Tee::PriHigh,"\n Choose the Window Number to be Shifted: \n");
        inputNumStr = pInterface->GetLine();
        for (sPanelWinNum = 0; sPanelWinNum < sourcePanel->winParams.size(); sPanelWinNum++)
        {
            if (sourcePanel->winParams[sPanelWinNum].windowNum == (UINT32) atoi(inputNumStr.c_str()))
                break;
        }
        Printf(Tee::PriHigh,"\n Choose the Destination Panel: \n");
        inputNumStr = pInterface->GetLine();
        if ((UINT32)atoi(inputNumStr.c_str()) < pDisplayPanels.size())
        {
            destPanel = pDisplayPanels[atoi(inputNumStr.c_str())];
        }

        CHECK_RC(pLwDisplay->SetWindowsOwnerDynamic(
                sourcePanel->head,
                1 << (sourcePanel->winParams[sPanelWinNum].windowNum),
                LwDisplay::WIN_OWNER_MODE_DETACH,
                LwDisplay::SEND_UPDATE,
                LwDisplay::WAIT_FOR_NOTIFIER));

        CHECK_RC(pLwDisplay->SetWindowsOwnerDynamic(
                destPanel->head,
                1 << (sourcePanel->winParams[sPanelWinNum].windowNum),
                LwDisplay::WIN_OWNER_MODE_ATTACH,
                LwDisplay::SEND_UPDATE,
                LwDisplay::WAIT_FOR_NOTIFIER));

        destPanel->winParams.push_back(sourcePanel->winParams[sPanelWinNum]);
        destPanel->windowSettings.push_back(sourcePanel->windowSettings[sPanelWinNum]);
        sourcePanel->winParams.erase(sourcePanel->winParams.begin() + sPanelWinNum);
        sourcePanel->windowSettings.erase(sourcePanel->windowSettings.begin() + sPanelWinNum);

        Printf(Tee::PriHigh,"\n Do you want to Exit: (y/n) \n");
        inputNumStr = pInterface->GetLine();
        if (inputNumStr.find("y") != string::npos)
            break;

    } while (true);

    delete pInterface;
    return rc;
}

DisplayPanel::~DisplayPanel()
{
    if (pModeset)
    {
        if (this->bActiveModeset)
        {
            pModeset->Detach();
        }
        pModeset->FreeWindowsAndSurface();

        delete pModeset;
        pModeset = NULL;
    }
}

bool DisplayPanel::HDRCapable(Display *m_pDisplay)
{
    RC rc;
    Edid    *pEdid = m_pDisplay->GetEdidMember();
    UINT08  rawEdid[EDID_SIZE_MAX];
    UINT32  rawEdidSize = EDID_SIZE_MAX;
    LWT_EDID_INFO   edidInfo;

    rc = pEdid->GetRawEdid(this->displayId, rawEdid, &rawEdidSize);

    // lets grab the LWT_EDID_INFO
    memset(&edidInfo, 0, sizeof(edidInfo));
    LwTiming_ParseEDIDInfo(rawEdid, rawEdidSize, &edidInfo);

    if (OK == rc && 
        edidInfo.hdr_static_metadata_info.supported_eotf.smpte_st_2084_eotf)
        return true;
    else
        return false;
}

Modeset::Modeset(DisplayPanel *pDisplayPanel, Display *pDisplay)
: m_pDisplayPanel(pDisplayPanel), m_pDisplay(pDisplay)
{

    RC rc;

    // Fill other information.
    m_pLwDisplay = m_pDisplay->GetLwDisplay();

    rc = m_pLwDisplay->GetLwDisplayChnContext(Display::CORE_CHANNEL_ID,
        (LwDispChnContext **)&m_pCoreChCtx,
        LwDisplay::ILWALID_WINDOW_INSTANCE,
        0);

    if (rc != OK)
    {
        Printf(Tee::PriError,"FAILED to get core context \n");
    }

    m_crcSettings.MemLocation = Memory::Coherent;

    if (OK != m_pLwDisplay->SetupCrcBuffer(&m_crcSettings))
    {
        Printf(Tee::PriError,"failed to setup of CRC buffer\n");
    }

    m_mode = ENABLE_BYPASS_SETTINGS;
    m_color = REC_709;
    m_modesetDone = false;
    m_semiPlanar = false;
    m_MaxHeads =  Utility::CountBits(m_pLwDisplay->m_UsableHeadsMask);
    m_MaxWindows = Utility::CountBits(m_pLwDisplay->m_UsableWindowsMask);
    m_MaxSors = Utility::CountBits(LwDisplay::GetUsableSORMask());
    m_IsHdrCapable = m_pLwDisplay->m_HdrCapable;
}

RC Modeset::SelectUserInputs(DisplayPanel *pDisplayPanel, bool& bExit)
{
    RC rc;
    vector<Display::Mode> listedModes;
    CHECK_RC(GetListedModes(&listedModes));
    vector<LWT_TIMING>lwtTimings;
    Printf(Tee::PriHigh, "\n DisplayId: 0x%X\n", (UINT32)m_pDisplayPanel->displayId);

    SimpleUserInterface * pInterface = new SimpleUserInterface(true);

    if ((pDisplayPanel->pModeset->GetPanelType() == LwDispPanelMgr::LWD_PANEL_TYPE_DVI) &&
        (!strcmp(pDisplayPanel->orProtocol.c_str(), "DUAL_TMDS")))
    {
        Printf(Tee::PriHigh, "\n");
        Printf(Tee::PriHigh, "********************************************************************************\n");
        Printf(Tee::PriHigh, "*                                                                              *\n");
        Printf(Tee::PriHigh, "* Modeset will use SINGLE_TMDS PROTOCOL if Pclk[10KHz] < 16500                 *\n");
        Printf(Tee::PriHigh, "* To check DUAL_TMDS, please select a resolution which will need Pclk > 16500  *\n");
        Printf(Tee::PriHigh, "*                                                                              *\n");
        Printf(Tee::PriHigh, "********************************************************************************\n");
        Printf(Tee::PriHigh, "\n");
    }

    Printf(Tee::PriHigh,"> Press Enter to continue \n");
    pInterface->GetLine();

    Printf(Tee::PriHigh, "Mode[0]: Width= 68; Height=52; depth = 32 PClk = 165000000\n");

    for (UINT32 mCnt = 0; mCnt < (UINT32)listedModes.size(); mCnt++)
    {
        LWT_TIMING timing;
        memset(&timing, 0 , sizeof(LWT_TIMING));
        if (pDisplayPanel->pModeset->GetPanelType() == LwDispPanelMgr::LWD_PANEL_TYPE_DSI)
        {
            rc = ModesetUtils::CallwlateLWTTiming(m_pDisplay,
                m_pDisplayPanel->displayId,
                listedModes[mCnt].width,
                listedModes[mCnt].height,
                listedModes[mCnt].refreshRate,
                Display::CVT_RB,
                &timing,
                0);
        }
        else
        {
            rc = ModesetUtils::CallwlateLWTTiming(m_pDisplay,
                m_pDisplayPanel->displayId,
                listedModes[mCnt].width,
                listedModes[mCnt].height,
                listedModes[mCnt].refreshRate,
                Display::AUTO,
                &timing,
                0);
        }
        if (rc != OK)
        {
            Printf(Tee::PriHigh,
                "ERROR: %s: CallwlateLWTTiming() failed to get backend timings. RC = %d.\n",
                __FUNCTION__, (UINT32)rc);
            CHECK_RC(rc);
        }
        lwtTimings.push_back(timing);
        Printf(Tee::PriHigh, "Mode[%d]: Width=%d; Height=%d; Depth=%d; RefreshRate=%d; PClk[10Khz] = %u.\n",
            mCnt + 1,
            listedModes[mCnt].width,
            listedModes[mCnt].height,
            listedModes[mCnt].depth,
            listedModes[mCnt].refreshRate,
            (UINT32)timing.pclk);
    }

    do
    {
        Printf(Tee::PriHigh,
            "> Input resolution index from above or type EXIT to exit\n");

        string inputNumStr = pInterface->GetLine();
        if (inputNumStr.find("EXIT") != string::npos)
        {
            bExit = true;
            break;
        }
        else
        {
            UINT32 index = atoi(inputNumStr.c_str());
            //
            // We are listing 68*52 mode by default.
            // So input index should be checked as
            // listedModes.size() + 1
            //
            if (index < (listedModes.size() + 1 ))
            {
                if (index == 0)
                {
                    pDisplayPanel->resolution.width = 68;
                    pDisplayPanel->resolution.height = 52;
                    pDisplayPanel->resolution.depth = 32;
                    pDisplayPanel->resolution.refreshrate = 60;
                }
                else
                {
                    index --;
                    pDisplayPanel->resolution.width = listedModes[index].width;
                    pDisplayPanel->resolution.height = listedModes[index].height;
                    pDisplayPanel->resolution.depth = listedModes[index].depth;
                    pDisplayPanel->resolution.refreshrate = listedModes[index].refreshRate;
                }
                Printf(Tee::PriHigh,
                    "\n > Input Head or Type 'D' for default value \n");

                inputNumStr = pInterface->GetLine();
                if (!(inputNumStr.find("D") != string::npos))
                {
                   pDisplayPanel->head = atoi(inputNumStr.c_str());
                }
                else
                {
                    // Get from Head Routing Map
                    UINT32 head = Display::ILWALID_HEAD;
                    CHECK_RC(m_pDisplay->GetHead(pDisplayPanel->displayId, &head));
                    pDisplayPanel->head = head;

                    Printf(Tee::PriHigh,
                        "\n %s : Using default Head Number: %d\n", __FUNCTION__, pDisplayPanel->head);
                }

                Printf(Tee::PriHigh,
                    "\n > Input SOR number or Type 'D' for default value \n");

                inputNumStr = pInterface->GetLine();

                if (!(inputNumStr.find("D") != string::npos))
                {
                    pDisplayPanel->sor =  atoi(inputNumStr.c_str());
                }
                else
                {
                    // Get From RM
                    UINT32 orIndex    = 0;
                    UINT32 orType     = 0;
                    UINT32 orProtocol = 0;
                    CHECK_RC(m_pDisplay->GetORProtocol(m_pDisplayPanel->displayId, &orIndex, &orType, &orProtocol));
                    pDisplayPanel->sor = orIndex;

                    Printf(Tee::PriHigh,
                        "\n %s : Using default SOR Number: %d\n", __FUNCTION__, pDisplayPanel->sor);
                }
                
                if (!m_pLwDisplay->m_DispCap.flexibleWinMapSupport)
                    Printf(Tee::PriHigh,
                        "\n >Volta:: Input Window Parameters in format windowNum-width-height, windowNum-width-height or type D for default \n");
                else
                    Printf(Tee::PriHigh,
                        "\n >From Turing:: Input Window Parameters in format headNum-windowNum-width-height, headNum-windowNum-width-height or type D for default\n");

                inputNumStr = pInterface->GetLine();
                if (inputNumStr.find("D") != string::npos)
                {
                    char buff[30];
                    memset(buff, 0, sizeof(buff));
                    // Append the head number first
                    if (m_pLwDisplay->m_DispCap.flexibleWinMapSupport)
                    {
                        sprintf(buff, "%d-", pDisplayPanel->head);
                    }
                    inputNumStr = string(buff);
                    sprintf(buff, "%d", pDisplayPanel->head * 2);
                    inputNumStr += string(buff);
                    sprintf(buff, "%d", pDisplayPanel->resolution.width);
                    inputNumStr += "-" + string(buff);
                    sprintf(buff, "%d", pDisplayPanel->resolution.height);
                    inputNumStr += "-" + string(buff);
                    inputNumStr += ",";
                    // Append the head number first
                    if (m_pLwDisplay->m_DispCap.flexibleWinMapSupport)
                    {
                        sprintf(buff, "%d-", pDisplayPanel->head);
                    }
                    inputNumStr += string(buff);
                    sprintf(buff, "%d", (pDisplayPanel->head * 2)+ 1);
                    inputNumStr += string(buff) + "-640-480"; // 640 x480 default size for second window

                    Printf(Tee::PriHigh,
                        "\n %s : Using default window params %s\n", __FUNCTION__, inputNumStr.c_str());
                }

                vector<string>windowStrs;
                vector<string>windowTokens;

                if (inputNumStr != "")
                {
                    windowStrs = DTIUtils::VerifUtils::Tokenize(inputNumStr, ",");
                }
                for (UINT32 i=0; i < windowStrs.size(); i++)
                {
                    windowTokens = DTIUtils::VerifUtils::Tokenize(windowStrs[i], "-");
                    WindowParam windowParam;

                    if (!m_pLwDisplay->m_DispCap.flexibleWinMapSupport)
                    {
                        if (windowTokens.size() == 3)
                        {
                            windowParam.windowNum = atoi(windowTokens[0].c_str());
                            windowParam.width = atoi(windowTokens[1].c_str());
                            windowParam.height = atoi(windowTokens[2].c_str());
                        }
                        else
                        {
                            Printf(Tee::PriHigh,"Wrong number of arguments passed.\n");
                            windowParam.windowNum = getMaxWindows() + 1;
                        }
                    }
                    else
                    {
                        if (windowTokens.size() == 4)
                        {
                            windowParam.headNum = atoi(windowTokens[0].c_str());
                            windowParam.windowNum = atoi(windowTokens[1].c_str());
                            windowParam.width = atoi(windowTokens[2].c_str());
                            windowParam.height = atoi(windowTokens[3].c_str());
                        }
                        else
                        {
                            Printf(Tee::PriHigh,"Wrong number of arguments passed.\n");
                            windowParam.windowNum = getMaxWindows() + 1;
                        }
                    }

                    if (windowParam.windowNum < getMaxWindows())
                    {
                        pDisplayPanel->winParams.push_back(windowParam);
                    }
                    else
                    {
                        Printf(Tee::PriHigh, "\n Wrong window inputs \n");
                    }
                }

                Printf(Tee::PriHigh,
                    "\n > Input Pixel Clock in HZ or Type 'D' for default value \n");

                inputNumStr = pInterface->GetLine();

                if (!(inputNumStr.find("D") != string::npos))
                {
                    pDisplayPanel->resolution.pixelClockHz =  atoi(inputNumStr.c_str());
                }
                else
                {
                    // Since the input is in KHZ
                    if ((pDisplayPanel->resolution.width == 68) &&
                        (pDisplayPanel->resolution.height == 52))
                    {
                        pDisplayPanel->resolution.pixelClockHz = 165000000;
                        pDisplayPanel->pixelDepth = LwDispORSettings::OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_24_444;
                    }
                    else
                    {
                        pDisplayPanel->resolution.pixelClockHz = lwtTimings[index].pclk * 10 * 1000;
                    }
                }

                LwDispHeadSettings &headSettings = m_pCoreChCtx->DispLwstomSettings.HeadSettings[pDisplayPanel->head];
                headSettings.Reset();
                LwDispRasterSettings &rasterSettings = headSettings.rasterSettings;

                rc = ModesetUtils::ColwertLwtTiming2EvoRasterSettings
                     (
                        lwtTimings[index],
                        &rasterSettings
                     );

                if (rc != OK)
                {
                    Printf(Tee::PriError, "%s failed to colwert into Raster settings index = %d",
                        __FUNCTION__, (index + 1));
                }

                rasterSettings.PixelClockHz = pDisplayPanel->resolution.pixelClockHz;
                rasterSettings.Dirty = true;

                if (pDisplayPanel->pModeset->IsPanelTypeDP())
                {
                    Modeset_DP* pDpModeset = (Modeset_DP*)pDisplayPanel->pModeset;
                    DisplayPortMgr::DP_LINK_CONFIG_DATA dpLinkConfig;

                    dpLinkConfig.linkRate = 0;
                    dpLinkConfig.laneCount = 0;
                    dpLinkConfig.bFec = false;

                    Printf(Tee::PriHigh,
                        "\n > Input DP Link Rate in Hz or Type 'D' for default value \n");
                    string inputLinkRate = pInterface->GetLine();
                    if (!(inputLinkRate.find("D") != string::npos))
                    {
                        dpLinkConfig.linkRate =  atoi(inputLinkRate.c_str());
                    }

                    Printf(Tee::PriHigh,
                        "\n > Input DP Lane Count or Type 'D' for default value \n");
                    string inputLaneCount = pInterface->GetLine();
                    if (!(inputLaneCount.find("D") != string::npos))
                    {
                        dpLinkConfig.laneCount =  atoi(inputLaneCount.c_str());
                    }

                    Printf(Tee::PriHigh,
                           "\n > Input FEC Enable Status(0 = FALSE / 1 = True) or Type 'D' for default value \n");
                    string fec = pInterface->GetLine();
                    if (!(fec.find("D") != string::npos))
                    {
                        dpLinkConfig.bFec = (atoi(fec.c_str()) != 0) ? true : false;
                    }

                    if(dpLinkConfig.linkRate || dpLinkConfig.laneCount || dpLinkConfig.bFec)
                    {
                        pDpModeset->SetupLinkConfig(dpLinkConfig);
                    }
                }

                rasterSettings.Print(Tee::PriHigh);

                Printf(Tee::PriHigh, "\n =========SELECTED ================= \n");
                Printf(Tee::PriHigh, " \n Display ID = 0x%x \n",(UINT32)pDisplayPanel->displayId);
                if (pDisplayPanel->pSecondaryPanel)
                {
                    Printf(Tee::PriHigh, "2-SST Secondary Display ID = 0x%x",(UINT32)pDisplayPanel->pSecondaryPanel->displayId);
                }

                Printf(Tee::PriHigh, "\n RasterParameters : Width = %d, Height = %d, Depth = %d, RefreshRate = %d, PixelClock = %d \n",
                    pDisplayPanel->resolution.width,
                    pDisplayPanel->resolution.height,
                    pDisplayPanel->resolution.depth,
                    pDisplayPanel->resolution.refreshrate,
                    pDisplayPanel->resolution.pixelClockHz
                    );

                Printf(Tee::PriHigh, "\n HEAD =  %d \n", pDisplayPanel->head);
                Printf(Tee::PriHigh, "\n SOR =  %d \n", pDisplayPanel->sor);
                Printf(Tee::PriHigh, "\n WINDOW PARAMETERS \n");
                for (UINT32 q=0; q < pDisplayPanel->winParams.size(); q++)
                {
                    Printf(Tee::PriHigh, "\n Window Num = %d \n",
                        pDisplayPanel->winParams[q].windowNum);
                    Printf(Tee::PriHigh, "\n Width = %d Height = %d \n",
                        pDisplayPanel->winParams[q].width, pDisplayPanel->winParams[q].height);
                }
                break;
            }
            else
            {
                Printf(Tee::PriHigh,
                    " Wrong Index type correct index \n");
            }
        }
    } while(true);

    delete pInterface;

    return rc;
}

RC Modeset::AllocateWindowsLwstomWinSettings(DisplayPanel *pDisplayPanel)
{
    RC rc = OK;
    for (UINT32 i = 0; i < pDisplayPanel->windowSettings.size(); i++)
    {
        // Allocate the window channel for the given window instance
        WindowIndex winIndex;
        CHECK_RC(m_pLwDisplay->AllocateWinAndWinImm(&winIndex,
            pDisplayPanel->head, pDisplayPanel->windowSettings[i].winSettings.winIndex));

        pDisplayPanel->windowSettings[i].winSettings.winIndex = winIndex;
    }
    return rc;
}

RC Modeset::AllocateWindowsAndSurface()
{
    RC rc;
    DTIUtils::ImageUtils imgReq;
    DTIUtils::ImageUtils imgArr;

    m_pDisplayPanel->windowSettings.resize(m_pDisplayPanel->winParams.size());

    for (UINT32 i = 0; i < m_pDisplayPanel->winParams.size(); i++)
    {
        //Reset window parameters before new allocation
        m_pDisplayPanel->windowSettings[i].winSettings.Reset();

        // Allocate the window channel for the given window instance
        // Get Head for Window Num
        WindowIndex winIndex;
        CHECK_RC(m_pLwDisplay->
            AllocateWinAndWinImm(&winIndex,
            m_pDisplayPanel->head,
            m_pDisplayPanel->winParams[i].windowNum));

        // Allocate default surface to the given window
        m_pDisplayPanel->windowSettings[i].winSettings.winIndex = winIndex;
        m_pDisplayPanel->windowSettings[i].winSettings.SetSizeOut(m_pDisplayPanel->winParams[i].width,
                              m_pDisplayPanel->winParams[i].height);

        m_pDisplayPanel->windowSettings[i].surfSettings.imageWidth =
            m_pDisplayPanel->windowSettings[i].winSettings.sizeOut.width;
        m_pDisplayPanel->windowSettings[i].surfSettings.imageHeight =
            m_pDisplayPanel->windowSettings[i].winSettings.sizeOut.height;
        m_pDisplayPanel->windowSettings[i].surfSettings.colorDepthBpp = m_pDisplayPanel->resolution.depth;
        m_pDisplayPanel->windowSettings[i].surfSettings.layout = Surface2D::Pitch;

        m_pDisplayPanel->windowSettings[i].headNum = m_pDisplayPanel->winParams[i].headNum;
        m_pDisplayPanel->windowSettings[i].windowNum = m_pDisplayPanel->winParams[i].windowNum;
        m_pDisplayPanel->windowSettings[i].inputLutLwrve = m_pDisplayPanel->winParams[i].inputLutLwrve;
        m_pDisplayPanel->windowSettings[i].tmoLutLwrve = m_pDisplayPanel->winParams[i].tmoLutLwrve;
        m_pDisplayPanel->windowSettings[i].winSettings.dirty = true;
        m_pDisplayPanel->windowSettings[i].winSettings.fmtLutSettings.dirty = true;
        m_pDisplayPanel->windowSettings[i].winSettings.bEnableByPass = false;
        if (!m_semiPlanar)
        {
            m_pDisplayPanel->windowSettings[i].winSettings.image.pkd.pWindowImage = new Surface2D;
        }
        else
        {
            m_pDisplayPanel->windowSettings[i].winSettings.image.semiPlanar.pYImage = new Surface2D;
            m_pDisplayPanel->windowSettings[i].winSettings.image.semiPlanar.pUVImage = new Surface2D;
            m_pDisplayPanel->windowSettings[i].winSettings.image.storageFormat = SurfaceUtils::SEMIPLANAR;
            // Y Surface settings
            m_pDisplayPanel->windowSettings[i].surfSettings.colorFormat = ColorUtils::Format::Y8;
            imgReq.ImageWidth  = m_pDisplayPanel->windowSettings[i].surfSettings.imageWidth;
            imgReq.ImageHeight = m_pDisplayPanel->windowSettings[i].surfSettings.imageHeight;
            imgReq.format      = DTIUtils::ImageFormat::IMAGE_RAW;
            DTIUtils::ImageUtils imgArr = DTIUtils::ImageUtils::SelectImage(&imgReq);
            m_pDisplayPanel->windowSettings[i].surfSettings.imageFileName = imgArr.GetImageName();
            m_pDisplayPanel->windowSettings[i].surfSettings.colorDepthBpp = 8;
            // UV surface settings
            m_pDisplayPanel->windowSettings[i].uvSurfSettings.colorFormat = ColorUtils::Format::V8U8;
            imgReq.ImageWidth  = m_pDisplayPanel->windowSettings[i].surfSettings.imageWidth/2;
            imgReq.ImageHeight = m_pDisplayPanel->windowSettings[i].surfSettings.imageHeight/2;
            imgReq.format      = DTIUtils::ImageFormat::IMAGE_RAW;
            imgArr = DTIUtils::ImageUtils::SelectImage(&imgReq);
            m_pDisplayPanel->windowSettings[i].uvSurfSettings.imageFileName = imgArr.GetImageName();
            m_pDisplayPanel->windowSettings[i].uvSurfSettings.imageWidth =
                                     m_pDisplayPanel->windowSettings[i].surfSettings.imageWidth/2;
            m_pDisplayPanel->windowSettings[i].uvSurfSettings.imageHeight = 
                                    m_pDisplayPanel->windowSettings[i].surfSettings.imageHeight/2;
            m_pDisplayPanel->windowSettings[i].uvSurfSettings.layout = Surface2D::Pitch;
            m_pDisplayPanel->windowSettings[i].uvSurfSettings.colorDepthBpp = 16;
            m_pDisplayPanel->windowSettings[i].winSettings.image.semiPlanar.colorFormat = ColorUtils::Y8___V8U8_N420;
        }

        if (ENABLE_HDR_SETTINGS == getDispMode())
        {
            if (m_pDisplayPanel->windowSettings[i].surfSettings.colorDepthBpp < 64)
            {
                Printf(Tee::PriHigh,
                       "AllocateWindowsAndSurface:Incorrect ColorDepth passed in commandline\n");
                //return RC::BAD_PARAMETER;
                m_pDisplayPanel->windowSettings[i].surfSettings.colorDepthBpp = 64;
            }
            imgReq.ImageWidth  = m_pDisplayPanel->windowSettings[i].surfSettings.imageWidth;
            imgReq.ImageHeight = m_pDisplayPanel->windowSettings[i].surfSettings.imageHeight;
            imgReq.format      = DTIUtils::ImageFormat::IMAGE_HDR;
            DTIUtils::ImageUtils imgArr = DTIUtils::ImageUtils::SelectImage(&imgReq);
            m_pDisplayPanel->windowSettings[i].surfSettings.imageFileName = imgArr.GetImageName();
            m_pDisplayPanel->windowSettings[i].surfSettings.colorFormat = ColorUtils::Format::RF16_GF16_BF16_AF16;
        }
        else
        {
            m_pDisplayPanel->windowSettings[i].winSettings.fmtLutSettings.dirty = true;

            imgArr = DTIUtils::ImageUtils::SelectImage(m_pDisplayPanel->windowSettings[i].surfSettings.imageWidth, 
                                            m_pDisplayPanel->windowSettings[i].surfSettings.imageHeight);
            m_pDisplayPanel->windowSettings[i].surfSettings.imageFileName = imgArr.GetImageName();    
        }
        if (ENABLE_BYPASS_SETTINGS == getDispMode())
        {
            m_pDisplayPanel->windowSettings[i].winSettings.bEnableByPass = true;
        }
    }

    return rc;
}

RC Modeset::UpdateWindowInputLut(LwDispWindowChnContext *pWinChnContext, UINT32 index)
{
    RC rc;

    pWinChnContext->WindowLwstomSettings.pInputLutSettings = new LwDispLutSettings();
    pWinChnContext->WindowLwstomSettings.pInputLutSettings->Disp30.dirty = true;

    CHECK_RC(m_pLwDisplay->SetupILutLwrve(
            &pWinChnContext->WindowLwstomSettings,
            m_pDisplayPanel->windowSettings[index].inputLutLwrve));

    pWinChnContext->WindowLwstomSettings.pInputLutSettings->Disp30.SetControlOutputLut(
                            LwDispLutSettings::Disp30Settings::INTERPOLATE_ENABLE,
                            LwDispLutSettings::Disp30Settings::MIRROR_DISABLE,
                            LwDispLutSettings::Disp30Settings::MODE_SEGMENTED);

    // Update the Panel Windowsettings as SetWindowImage can use it later.
    m_pDisplayPanel->windowSettings[index].winSettings.pInputLutSettings = 
                    pWinChnContext->WindowLwstomSettings.pInputLutSettings;

    return rc;
}

RC Modeset::UpdateWindowTmoLut(LwDispWindowChnContext *pWinChnContext, UINT32 index)
{
    RC rc;

    pWinChnContext->WindowLwstomSettings.colorSpace = LwDispWindowSettings::YUV_2100;
    m_pLwDisplay->SetCscLutsAndMatrices(&pWinChnContext->WindowLwstomSettings);

    pWinChnContext->WindowLwstomSettings.pTmoLutSettings = new LwDispLutSettings();
    pWinChnContext->WindowLwstomSettings.pTmoLutSettings->Disp30.dirty = true;

    CHECK_RC(m_pLwDisplay->SetupTmoLutLwrve(
            &pWinChnContext->WindowLwstomSettings,
            m_pDisplayPanel->windowSettings[index].tmoLutLwrve));

    pWinChnContext->WindowLwstomSettings.pTmoLutSettings->Disp30.lutSize = 1029;

    pWinChnContext->WindowLwstomSettings.pTmoLutSettings->Disp30.LutCtrlInterpolate = 
                                    LwDispLutSettings::Disp30Settings::INTERPOLATE_ENABLE;

    // Update the Panel Windowsettings as SetWindowImage can use it later.
    m_pDisplayPanel->windowSettings[index].winSettings.pTmoLutSettings = 
                    pWinChnContext->WindowLwstomSettings.pTmoLutSettings;

    return rc;
}

RC Modeset::UpdateHeadOutputLut()
{
    RC rc;
    LwDispHeadSettings &headSettings = (m_pCoreChCtx->DispLwstomSettings.HeadSettings[m_pDisplayPanel->head]);

    headSettings.pOutputLutSettings = new LwDispLutSettings();

    CHECK_RC(m_pLwDisplay->SetupOLutLwrve(
                &headSettings,
                m_pDisplayPanel->outputLutLwrve));
    headSettings.combinedHeadBounds.usageBounds.oLutUsage =
                                    LwDispHeadUsageBounds::OLUT_ALLOWED_TRUE;

    headSettings.pOutputLutSettings->Disp30.SetControlOutputLut(
                                LwDispLutSettings::Disp30Settings::INTERPOLATE_ENABLE,
                                LwDispLutSettings::Disp30Settings::MIRROR_DISABLE,
                                LwDispLutSettings::Disp30Settings::MODE_SEGMENTED);
    return rc;
}

RC Modeset::UpdateWindowsAndHeadSettings()
{
    RC rc;
    LwDispWindowChnContext   *pWinChnContext;
    UINT32 usableWindowsMask = 0;

    for (auto &winParam: m_pDisplayPanel->winParams)
    {
        usableWindowsMask |= (1 << winParam.windowNum);
    }

    // Window Updates
    while (usableWindowsMask)
    {
        UINT32 windowIdx = Utility::BitScanForward(usableWindowsMask);
        UINT32 i;
        usableWindowsMask ^= 1 << windowIdx;
        for (i = 0; i < m_pDisplayPanel->windowSettings.size(); i++)
        {
            if (m_pDisplayPanel->windowSettings[i].windowNum == windowIdx)
            {
                break;
            }
        }
        CHECK_RC(m_pLwDisplay->GetLwDisplayChnContext(Display::WINDOW_CHANNEL_ID,
                (LwDispChnContext **)&pWinChnContext,
                windowIdx,
                m_pDisplayPanel->windowSettings[i].headNum));
        pWinChnContext->WindowLwstomSettings.winIndex = 
                            m_pDisplayPanel->windowSettings[i].winSettings.winIndex;
        pWinChnContext->WindowLwstomSettings.dirty =
                            m_pDisplayPanel->windowSettings[i].winSettings.dirty;
        pWinChnContext->WindowLwstomSettings.fmtLutSettings.dirty = 
                    m_pDisplayPanel->windowSettings[i].winSettings.fmtLutSettings.dirty;
        pWinChnContext->WindowLwstomSettings.bEnableByPass =
                    m_pDisplayPanel->windowSettings[i].winSettings.bEnableByPass;

        CHECK_RC(m_pLwDisplay->SetWindowsOwnerDynamic(
                    m_pDisplayPanel->windowSettings[i].headNum,
                    1 << windowIdx,
                    LwDisplay::WIN_OWNER_MODE_ATTACH,
                    LwDisplay::SEND_UPDATE,
                    LwDisplay::WAIT_FOR_NOTIFIER));

        if (getDispMode() == ENABLE_BYPASS_SETTINGS)
        {
            continue;
        }

        if (m_pDisplayPanel->windowSettings[i].inputLutLwrve !=
                EotfLutGenerator::EOTF_LWRVE_NONE)
        {
            UpdateWindowInputLut(pWinChnContext, i);
        }

        if ((m_pDisplayPanel->windowSettings[i].tmoLutLwrve !=
                EetfLutGenerator::EETF_LWRVE_NONE) &&
             m_pLwDisplay->m_WindowCapabilities[m_pDisplayPanel->windowSettings[i].windowNum].fullHdrPipe
            )
        {
            UpdateWindowTmoLut(pWinChnContext, i);
        }
    }

    if (m_pDisplayPanel->outputLutLwrve !=
                OetfLutGenerator::OETF_LWRVE_NONE)
    {
        UpdateHeadOutputLut();
    }

    return rc;
}

RC Modeset::SetupChannelImage()
{
    RC rc;

    if (IsHdrCapable())
    {
        CHECK_RC(UpdateWindowsAndHeadSettings());
    }

    for (UINT32 i = 0; i < m_pDisplayPanel->windowSettings.size(); i++)
    {
        if (!m_semiPlanar)
        {
            CHECK_RC(SurfaceUtils::SurfaceHelper::SetupSurface(
                m_pDisplay->GetOwningDisplaySubdevice(),
                &(m_pDisplayPanel->windowSettings[i].surfSettings),
                m_pDisplayPanel->windowSettings[i].winSettings.image.pkd.pWindowImage,
                Display::WINDOW_CHANNEL_ID));
        }
        else
        {
            CHECK_RC(SurfaceUtils::SurfaceHelper::SetupSurface(
                m_pDisplay->GetOwningDisplaySubdevice(),
                &(m_pDisplayPanel->windowSettings[i].surfSettings),
                m_pDisplayPanel->windowSettings[i].winSettings.image.semiPlanar.pYImage,
                Display::WINDOW_CHANNEL_ID));

            CHECK_RC(SurfaceUtils::SurfaceHelper::SetupSurface(
                m_pDisplay->GetOwningDisplaySubdevice(),
                &(m_pDisplayPanel->windowSettings[i].uvSurfSettings),
                m_pDisplayPanel->windowSettings[i].winSettings.image.semiPlanar.pUVImage,
                Display::WINDOW_CHANNEL_ID));
        }

        CHECK_RC(m_pLwDisplay->SetupChannelImage(m_pDisplayPanel->displayId,
              &(m_pDisplayPanel->windowSettings[i].winSettings)));
    }
    return rc;
}

RC Modeset::SetWindowImage(DisplayPanel *pDisplayPanel)
{
    RC rc;
    for (UINT32 i = 0; i < m_pDisplayPanel->winParams.size(); i++)
    {
        CHECK_RC(m_pLwDisplay->SetWindowImage(&(m_pDisplayPanel->windowSettings[i].winSettings),
                                          m_pDisplayPanel->windowSettings[i].surfSettings.imageWidth,
                                          m_pDisplayPanel->windowSettings[i].surfSettings.imageHeight,
                                          LwDisplay::SKIP_UPDATE,
                                          LwDisplay::DONT_WAIT_FOR_NOTIFIER));
    }

    return rc;
}

RC Modeset::SendInterlockedUpdates(DisplayPanel *pDisplayPanel)
{
    RC rc;
    LwDispWindowSettings *pWinSettings = &(m_pDisplayPanel->windowSettings[0].winSettings);

    CHECK_RC(m_pLwDisplay->SendInterlockedUpdates(
            LwDisplay::UPDATE_CORE,
            1 << pWinSettings->winIndex,
            0,
            0,
            0,
            LwDisplay::DONT_INTERLOCK_CHANNELS,
            LwDisplay::WAIT_FOR_NOTIFIER));

    return rc;
}

RC Modeset::GetListedModes(vector<Display::Mode>*pListedModes)
{
    RC rc;
    if (m_pDisplayPanel->pModeset->GetPanelType() == LwDispPanelMgr::LWD_PANEL_TYPE_DSI)
    {
        Display::Mode mode;

        CHECK_RC(m_pDisplay->GetDsiModeTimings(
                        m_pDisplayPanel->displayId, &mode));

        /* RM DSI specifies bpp as 24bpp, but the test expects 32bpp to identify window format */
        mode.depth += mode.depth/3;
        pListedModes->push_back(mode);
    }
    else
    {
        // getListedResolutions & check get 1 res above 340Mhz.
        CHECK_RC(DTIUtils::EDIDUtils::GetListedModes(m_pDisplay,
                                        m_pDisplayPanel->displayId,
                                        pListedModes,
                                        false));
     }

   return rc;
}

RC Modeset::FreeWindowsAndSurface()
{
    RC rc;
    for (UINT32 i = 0; i < m_pDisplayPanel->winParams.size(); i++)
    {
        if (m_pDisplayPanel->windowSettings[i].winSettings.image.pkd.pWindowImage &&
            m_pDisplayPanel->windowSettings[i].winSettings.image.pkd.pWindowImage->GetMemHandle())
        {
            m_pDisplayPanel->windowSettings[i].winSettings.image.pkd.pWindowImage->Free();
            delete m_pDisplayPanel->windowSettings[i].winSettings.image.pkd.pWindowImage;
            m_pDisplayPanel->windowSettings[i].winSettings.image.pkd.pWindowImage = NULL;
        }

        if (m_pDisplayPanel->windowSettings[i].winSettings.image.semiPlanar.pYImage &&
            m_pDisplayPanel->windowSettings[i].winSettings.image.semiPlanar.pYImage->GetMemHandle())
        {
            m_pDisplayPanel->windowSettings[i].winSettings.image.semiPlanar.pUVImage->Free();
            delete m_pDisplayPanel->windowSettings[i].winSettings.image.semiPlanar.pYImage;
            m_pDisplayPanel->windowSettings[i].winSettings.image.semiPlanar.pYImage = NULL;
        }

        if (m_pDisplayPanel->windowSettings[i].winSettings.image.semiPlanar.pUVImage &&
            m_pDisplayPanel->windowSettings[i].winSettings.image.semiPlanar.pUVImage->GetMemHandle())
        {
            m_pDisplayPanel->windowSettings[i].winSettings.image.semiPlanar.pUVImage->Free();
            delete m_pDisplayPanel->windowSettings[i].winSettings.image.semiPlanar.pUVImage;
            m_pDisplayPanel->windowSettings[i].winSettings.image.semiPlanar.pUVImage = NULL;
        }

        if (m_pDisplayPanel->bValidLwrsorSettings)
        {
            if (m_pDisplayPanel->lwrsorSettings.pLwrsorSurface->GetMemHandle())
                m_pDisplayPanel->lwrsorSettings.pLwrsorSurface->Free();
            delete m_pDisplayPanel->lwrsorSettings.pLwrsorSurface;
            m_pDisplayPanel->lwrsorSettings.pLwrsorSurface = NULL;
            // Deallocate cursor immediate for this head
            m_pLwDisplay->DestroyLwrsorImm(m_pDisplayPanel->head);
        }
        // Deallocate Window channel and assocaited window images
        CHECK_RC(m_pLwDisplay->DestroyWinAndWinImm(
            m_pDisplayPanel->windowSettings[i].winSettings.winIndex));

    }
    return rc;
}

RC Modeset::AllocateLwrsorImm()
{
    RC rc;

    CHECK_RC(m_pLwDisplay->AllocateLwrsorImm(m_pDisplayPanel->head));

    return rc;
}

RC Modeset::SetupLwrsorChannelImage()
{
    RC rc;

    m_pDisplayPanel->lwrsorSettings.hotSpotX = 0;
    m_pDisplayPanel->lwrsorSettings.hotSpotY = 0;
    m_pDisplayPanel->lwrsorSettings.hotSpotPointOutX = 0;
    m_pDisplayPanel->lwrsorSettings.hotSpotPointOutY = 0;

    if (!m_pDisplayPanel->lwrsorSettings.pLwrsorSurface)
        m_pDisplayPanel->lwrsorSettings.pLwrsorSurface = new Surface2D;

    if (!m_pDisplayPanel->lwrsorSettings.pLwrsorSurface)
        return RC::SOFTWARE_ERROR;

    CHECK_RC(SurfaceUtils::SurfaceHelper::SetupSurface(
            m_pDisplay->GetOwningDisplaySubdevice(),
            &m_pDisplayPanel->lwrsorSurfParams,
            m_pDisplayPanel->lwrsorSettings.pLwrsorSurface,
            Display::LWRSOR_IMM_CHANNEL_ID));

    CHECK_RC(m_pLwDisplay->SetupLwrsorChannelImage(
             m_pDisplayPanel->displayId,
             &m_pDisplayPanel->lwrsorSettings,
             m_pDisplayPanel->head));

    CHECK_RC(m_pLwDisplay->SetLwrsorImage(
             m_pDisplayPanel->head,
             &m_pDisplayPanel->lwrsorSettings,
             LwDisplay::SKIP_UPDATE));

    return rc;
}

RC Modeset::SetVRRSettings(bool enable, bool bLegacy)
{
    RC rc;
    LwDispHeadSettings &headSettings = (m_pCoreChCtx->DispLwstomSettings.HeadSettings[m_pDisplayPanel->head]);

    LW0073_CTRL_SYSTEM_GET_VRR_CONFIG_PARAMS vrrConfig = {0};
    vrrConfig.subDeviceInstance = 0;

    CHECK_RC_MSG(m_pLwDisplay->RmControl(LW0073_CTRL_CMD_SYSTEM_GET_VRR_CONFIG,
        &vrrConfig, sizeof(vrrConfig)), "Unable to get VRR config\n");

    if (vrrConfig.bCapable == false)
    {
        Printf(Tee::PriHigh, "\n ERROR: VRR NOT SUPPORTED\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    // Setup core channel methods
    if (true == enable)
    {
        Printf(Tee::PriHigh, "Modeset::%s -> VRR settings ENABLED - setup OneShotMode on head(%d)\n",
            __FUNCTION__, m_pDisplayPanel->head);
        headSettings.stallLockSettings.dirty = true;
        headSettings.stallLockSettings.bStallLockEnable = true;
        headSettings.stallLockSettings.bOneShotMode = true;
        headSettings.stallLockSettings.bExternalTriggerPin = false;
        headSettings.stallLockSettings.bLineLock = true;
        headSettings.stallLockSettings.vrrFrameLockPin = 0;

        headSettings.displayRateSettings.dirty = true;
        headSettings.displayRateSettings.minRefreshIntervalUs = 0;

        if (bLegacy)
        {
            // Register based VRR
            Printf(Tee::PriHigh, "Modeset::%s -> VRR settings - Legacy VRR\n", __FUNCTION__);
            headSettings.displayRateSettings.bOneShotModeEnable = false;
        }
        else
        {
            // Method based VRR
            Printf(Tee::PriHigh, "Modeset::%s -> VRR settings - Method based VRR\n", __FUNCTION__);
            headSettings.displayRateSettings.bOneShotModeEnable = true;
        }
    }
    else
    {
        Printf(Tee::PriHigh, "Modeset::%s -> VRR settings DISABLED\n", __FUNCTION__);
        headSettings.stallLockSettings.dirty = true;
        headSettings.stallLockSettings.bStallLockEnable = false;
        headSettings.stallLockSettings.bOneShotMode = false;
        headSettings.stallLockSettings.bExternalTriggerPin = false;
        headSettings.stallLockSettings.bLineLock = true;
        headSettings.stallLockSettings.vrrFrameLockPin = 0;

        headSettings.displayRateSettings.dirty = true;
        headSettings.displayRateSettings.bOneShotModeEnable = false;
        headSettings.displayRateSettings.minRefreshIntervalUs = 0;
    }

    LW0073_CTRL_SYSTEM_ARM_LIGHTWEIGHT_SUPERVISOR_PARAMS armLWSVParams = {0};
    armLWSVParams.subDeviceInstance = 0;
    armLWSVParams.displayId = m_pDisplayPanel->displayId;
    armLWSVParams.bArmLWSV = true;
    armLWSVParams.bVrrState = true;

    CHECK_RC_MSG(m_pLwDisplay->RmControl(LW0073_CTRL_CMD_SYSTEM_ARM_LIGHTWEIGHT_SUPERVISOR,
        &armLWSVParams, sizeof(armLWSVParams)), "Unable to ARM LIGHTWEIGHT SUPERVISOR\n");

    CHECK_RC(m_pLwDisplay->UpdateVRR(m_pDisplayPanel->head));

    armLWSVParams.bArmLWSV = false;
    CHECK_RC_MSG(m_pLwDisplay->RmControl(LW0073_CTRL_CMD_SYSTEM_ARM_LIGHTWEIGHT_SUPERVISOR,
        &armLWSVParams, sizeof(armLWSVParams)), "Unable to ARM LIGHTWEIGHT SUPERVISOR\n");

    return rc;
}

RC Modeset::DisableVRR(DisplayPanel *pDisplayPanel)
{
    RC rc;
    LW0073_CTRL_SYSTEM_SET_VRR_PARAMS setVrrParams = {0};
    LW0073_CTRL_SYSTEM_CLEAR_ELV_BLOCK_PARAMS clearELVBlockParams = {0};

    clearELVBlockParams.subDeviceInstance = m_pDisplay->GetOwningDisplaySubdevice()->GetSubdeviceInst();
    clearELVBlockParams.displayId = m_pDisplayPanel->displayId;

    CHECK_RC_MSG(m_pDisplay->RmControl(LW0073_CTRL_CMD_SYSTEM_CLEAR_ELV_BLOCK,
        &clearELVBlockParams, sizeof(clearELVBlockParams)), "Unable to clear ELV\n");

    // Disable VRR
    setVrrParams.subDeviceInstance = m_pDisplay->GetOwningDisplaySubdevice()->GetSubdeviceInst();
    setVrrParams.bEnable = LW_FALSE;

    CHECK_RC_MSG(m_pDisplay->RmControl(LW0073_CTRL_CMD_SYSTEM_SET_VRR,
        &setVrrParams, sizeof(setVrrParams)), "Unable to enable the VRR\n");

    return rc;
}

RC Modeset::EnableVRR(DisplayPanel *pDisplayPanel, bool bLegacy)
{
    RC rc;
    LW0073_CTRL_SYSTEM_SET_VRR_PARAMS vrrParams = {0};
    LW0073_CTRL_SYSTEM_GET_VRR_CONFIG_PARAMS vrrConfig = {0};
    LW0073_CTRL_SYSTEM_MODIFY_VRR_HEAD_GROUP_PARAMS params = {0};
    LW0073_CTRL_SYSTEM_ADD_VRR_HEAD_PARAMS addVrrHeadParams = {0};
    LW0073_CTRL_SYSTEM_MODIFY_VRR_HEAD_PARAMS *modifyVrrHeadparams;

    // Get VRR info
    vrrConfig.subDeviceInstance = m_pDisplay->GetOwningDisplaySubdevice()->GetSubdeviceInst();
    CHECK_RC_MSG(m_pDisplay->RmControl(LW0073_CTRL_CMD_SYSTEM_GET_VRR_CONFIG,
        &vrrConfig, sizeof(vrrConfig)), "Unable to get VRR config\n");

    if (vrrConfig.bCapable == false)
    {
        Printf(Tee::PriHigh, "\n ERROR: VRR NOT SUPPORTED\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    if (bLegacy)
    {
        // Enabled VRR - Adds trap handler in RM
        addVrrHeadParams.subDeviceInstance =
            m_pDisplay->GetOwningDisplaySubdevice()->GetSubdeviceInst();
        addVrrHeadParams.displayId = pDisplayPanel->displayId;
        addVrrHeadParams.bBlockChannel = LW_TRUE;

        CHECK_RC(pDisplayPanel->windowSettings[0].winSettings.image.pkd.pWindowImage->Map(m_pDisplay->GetOwningDisplaySubdevice()->GetSubdeviceInst()));
        addVrrHeadParams.frameSemaphore = LW_PTR_TO_LwP64(pDisplayPanel->windowSettings[0].winSettings.image.pkd.pWindowImage->GetAddress());
        addVrrHeadParams.frameSemaphoreCount = 1;

        addVrrHeadParams.bOneShotMode = LW_TRUE;
        addVrrHeadParams.bExternalFramelockToggle = LW_FALSE;
        addVrrHeadParams.externalFramelockTimeout = 0;
        addVrrHeadParams.bEnableCrashSync = LW_FALSE;
        addVrrHeadParams.winChInstance = (pDisplayPanel->head << 1);

        CHECK_RC_MSG(m_pDisplay->RmControl(LW0073_CTRL_CMD_SYSTEM_ADD_VRR_HEAD,
            &addVrrHeadParams, sizeof(addVrrHeadParams)), "Unable to configure the VRR\n");
    }

    // Enable VRR
    Printf(Tee::PriHigh, "%s: Enable %s based VRR\n", __FUNCTION__,
        bLegacy ? "Register":"Method");

    if (!bLegacy)
    {
        vrrParams.bUsingMethodBased = LW_TRUE;
    }

    vrrParams.subDeviceInstance = m_pDisplay->GetOwningDisplaySubdevice()->GetSubdeviceInst();
    vrrParams.bEnable = LW_TRUE;
    vrrParams.displayId = pDisplayPanel->displayId;

    CHECK_RC_MSG(m_pDisplay->RmControl(LW0073_CTRL_CMD_SYSTEM_SET_VRR,
        &vrrParams, sizeof(vrrParams)), "Unable to enable the VRR\n");

    if (bLegacy)
    {
        // Modify VRR state to active
        modifyVrrHeadparams = &params.modifyVrrHeadParams[0];
        modifyVrrHeadparams->subDeviceInstance = m_pDisplay->GetOwningDisplaySubdevice()->GetSubdeviceInst();
        modifyVrrHeadparams->displayId = pDisplayPanel->displayId;
        modifyVrrHeadparams->bBlockChannel = LW_TRUE;
        modifyVrrHeadparams->bClearSemaphores = LW_FALSE;
        modifyVrrHeadparams->bSetSuspended = LW_FALSE;
        modifyVrrHeadparams->backPorchExtension = 0;
        modifyVrrHeadparams->frontPorchExtension = 0;
        params.numHeads = 1;

        CHECK_RC_MSG(m_pDisplay->RmControl(LW0073_CTRL_CMD_SYSTEM_MODIFY_VRR_HEAD_GROUP,
            &params, sizeof(params)), "Unable to set VRR state\n");
    }

    return rc;
}

RC Modeset::ForceDSCBitsPerPixel(UINT32 forceDscBitsPerPixelX16)
{
    RC rc;
    LwDispHeadSettings &headSettings = (m_pCoreChCtx->DispLwstomSettings.HeadSettings[m_pDisplayPanel->head]);
    
    headSettings.dscSettings.Reset();
    headSettings.dscSettings.dscMode = Display::DSCMode::SINGLE;
    headSettings.dscSettings.dscParams.bitsPerPixelX16 = forceDscBitsPerPixelX16;
    
    return rc;
}

RC Modeset::ForceYUV(Display::ColorSpace colorSpace)
{
    RC rc;
    LwDispHeadSettings &headSettings = (m_pCoreChCtx->DispLwstomSettings.HeadSettings[m_pDisplayPanel->head]);
   
    switch(colorSpace)
    {
        case Display::COLORSPACE_RGB:
            return rc;
        case Display::COLORSPACE_YCBCR420:
            headSettings.procampSettings.ChromaDowlw = LwDispProcampSettings::CHROMA_DOWN_V::DOWN_V_ENABLE;
            headSettings.controlSettings.yuv420packer = LwDispControlSettings::YUV420PACKER::ENABLE; 
            headSettings.controlSettings.dirty = true; 
        case Display::COLORSPACE_YCBCR422:
            headSettings.procampSettings.ChromaLpf = LwDispProcampSettings::CHROMA_LPF::ENABLE;
        case Display::COLORSPACE_YCBCR444:
            headSettings.procampSettings.ColorSpace = Display::ORColorSpace::YUV_709;
            headSettings.procampSettings.dirty = true;
            break;
        default:
            Printf(Tee::PriHigh, "Invalid Output Colorformat \n");
            return RC::SOFTWARE_ERROR;
    } 
    return rc;
}

RC Modeset::SetLwstomSettings()
{

    RC rc;
    LwDispHeadSettings &headSettings = (m_pCoreChCtx->DispLwstomSettings.HeadSettings[m_pDisplayPanel->head]);

    // Others will set as default.
    // Here edid file name to be supplied do that as next change
    if (m_pDisplayPanel->sor != DEFAULT_PANEL_VALUE)
    {
        if (m_pDisplay->GetOwningDisplaySubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_SOR_XBAR))
        {
            UINT32 sorExcludeMask = 0;
            vector<UINT32>sorList;
            sorExcludeMask = (0xf)^(BIT(m_pDisplayPanel->sor));

            if ((m_pDisplayPanel->orProtocol.compare("EDP") == 0) || (m_pDisplayPanel->orProtocol.compare("eDP") == 0))
            {
                sorExcludeMask = 0;
            }
            DisplayIDs monitorIDs;
            bool isMST = m_pDisplay->IsMultiStreamDisplay(m_pDisplayPanel->displayId.Get());

            if (!isMST)
            {
                monitorIDs.push_back(m_pDisplayPanel->displayId);
            }
            else
            {
                UINT32 connID = 0;
                m_pDisplay->GetDisplayPortMgr()->GetConnectorID(m_pDisplayPanel->displayId.Get(), &connID);
                monitorIDs.push_back(DisplayID(connID));
            }

            if (m_pDisplayPanel->pSecondaryPanel && !isMST)
            {
                monitorIDs.push_back(m_pDisplayPanel->pSecondaryPanel->displayId);

            }

            CHECK_RC(m_pDisplay->AssignSOR(monitorIDs, sorExcludeMask,
                sorList, Display::AssignSorSetting::ONE_HEAD_MODE));
            bool foundSor   = false;
            UINT32 availSor = DEFAULT_PANEL_VALUE;

            for (UINT32 sorNum = 0; sorNum < getMaxSors(); sorNum++)
            {
                DisplayIDs sorDisplays;
                m_pDisplay->DispMaskToDispID(sorList[sorNum], sorDisplays);
                if (m_pDisplay->IsDispAvailInList(monitorIDs, sorDisplays))
                {
                    if(sorNum == m_pDisplayPanel->sor)
                    {
                        foundSor = true;
                        break;
                    }
                    availSor = sorNum;
                }
            }
            if(!foundSor)
            {
                Printf(Tee::PriHigh, "%s: SOR %u cannot be forced on DisplayID: 0x%x\n", __FUNCTION__, m_pDisplayPanel->sor, m_pDisplayPanel->displayId.Get());
                if(availSor == DEFAULT_PANEL_VALUE)
                {
                    Printf(Tee::PriHigh, "No available SOR\n");
                    return RC::SOFTWARE_ERROR;
                }
                else
                {
                    Printf(Tee::PriHigh, "Using SOR = %d\n", availSor);
                    m_pDisplayPanel->sor = availSor;
                }
            }
        }
        else
        {
            UINT32 orIndex    = 0;
            UINT32 orType     = 0;
            UINT32 orProtocol = 0;

            CHECK_RC(m_pDisplay->GetORProtocol(m_pDisplayPanel->displayId, &orIndex, &orType, &orProtocol));
            if (orIndex != m_pDisplayPanel->sor)
            {
                Printf(Tee::PriHigh,
                "\n SOR number %d : can't be forced on DisplayID = 0x%X \n", m_pDisplayPanel->sor,
                (UINT32)m_pDisplayPanel->displayId);

                m_pDisplayPanel->sor = orIndex;

                Printf(Tee::PriHigh,
                    "\n New SOR Number = %d DisplayID = 0x%X \n",
                    m_pDisplayPanel->sor,
                    (UINT32)m_pDisplayPanel->displayId);
            }

        }
    }

    if (m_pDisplayPanel->sor != DEFAULT_PANEL_VALUE)
    {
        LwDispORSettings *pORSettings = NULL;

        if (m_pDisplayPanel->pModeset->GetPanelType() == LwDispPanelMgr::LWD_PANEL_TYPE_DSI)
        {
            pORSettings = &(m_pCoreChCtx->DispLwstomSettings.DsiSettings[m_pDisplayPanel->sor]);
        }
        else
        {
            pORSettings = &(m_pCoreChCtx->DispLwstomSettings.SorSettings[m_pDisplayPanel->sor]);
        }
        if (m_pDisplayPanel->pixelDepth != LwDispORSettings::OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_ILWALID)
        {
            pORSettings->pixelDepth = m_pDisplayPanel->pixelDepth;
        }
        else
        {
            if (m_pDisplayPanel->isFakeDisplay)
            {
                pORSettings->pixelDepth = LwDispORSettings::OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_24_444;
            }
            else
            {
                RC rcPd = m_pLwDisplay->GetPixelDepth(m_pDisplayPanel->displayId, &pORSettings->pixelDepth);
                if (rcPd == RC::SOFTWARE_ERROR)
                {
                    pORSettings->pixelDepth = LwDispORSettings::OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_24_444;
                }
            }
        }
    }

     if ((m_pDisplayPanel->resolution.width == 68) &&
         (m_pDisplayPanel->resolution.height == 52))
     {
         headSettings.rasterSettings.RasterWidth = 80;
         headSettings.rasterSettings.ActiveX = 68;
         headSettings.rasterSettings.SyncEndX = 5;
         headSettings.rasterSettings.BlankStartX = 76;
         headSettings.rasterSettings.BlankEndX = 8;

         headSettings.rasterSettings.RasterHeight = 60;
         headSettings.rasterSettings.ActiveY = 52;
         headSettings.rasterSettings.SyncEndY = 1;
         headSettings.rasterSettings.BlankStartY = 55;
         headSettings.rasterSettings.BlankEndY = 3;
         headSettings.rasterSettings.Dirty = true;
         m_pDisplayPanel->resolution.pixelClockHz = 165000000;
     }

    if (m_pDisplayPanel->resolution.pixelClockHz != DEFAULT_PANEL_VALUE)
    {
        Printf(Tee::PriHigh, "In %s() Using PixelClockHz = %d \n", __FUNCTION__, m_pDisplayPanel->resolution.pixelClockHz);
        headSettings.pixelClkSettings.pixelClkFreqHz = m_pDisplayPanel->resolution.pixelClockHz;
        headSettings.pixelClkSettings.bAdj1000Div1001 = false;
        headSettings.pixelClkSettings.bNotDriver = false;
        headSettings.pixelClkSettings.bHopping = false;
        headSettings.pixelClkSettings.HoppingMode = LwDispPixelClockSettings::VBLANK;
        headSettings.pixelClkSettings.dirty = true;
    }
    return rc;
}

RC Modeset::ClearLwstomSettings()
{
    RC rc;
    LwDispHeadSettings &headSettings = (m_pCoreChCtx->DispLwstomSettings.HeadSettings[m_pDisplayPanel->head]);
    headSettings.Reset();
    return rc;
}

#if defined(TEGRA_MODS)
UINT32 Modeset::Disp_reg_dump(UINT32 offset)
{
    UINT32 fd = open ("/dev/mem", O_SYNC | O_RDWR);
    UINT32 disp_size = 0x100000;
    UINT08 *disp_base_addr = (UINT08 *)mmap (0, disp_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0x13800000);
    UINT32 reg_val;
    reg_val = *((UINT32*)(disp_base_addr + offset));
    munmap((void*)disp_base_addr, disp_size);
    close(fd);
    return reg_val;
}
#endif

Modeset_TMDS::Modeset_TMDS(DisplayPanel *pDisplayPanel, Display *pDisplay): Modeset(pDisplayPanel, pDisplay)
{
    RC rc;
    m_panelType = LwDispPanelMgr::LWD_PANEL_TYPE_DVI;
    if (!(DTIUtils::EDIDUtils::IsValidEdid(m_pDisplay, m_pDisplayPanel->displayId) &&
        DTIUtils::EDIDUtils::EdidHasResolutions(m_pDisplay, m_pDisplayPanel->displayId)))
    {
        if((rc = DTIUtils::EDIDUtils::SetLwstomEdid(m_pDisplay, m_pDisplayPanel->displayId, !m_pDisplayPanel->isFakeDisplay)) != OK)
        {
            Printf(Tee::PriHigh,"SetLwstomEdid failed for displayID = 0x%X \n",
                (UINT32)pDisplayPanel->displayId);
        }
    }
}

Modeset_TMDS::~Modeset_TMDS()
{
    // Detach fake devices here.

}

RC Modeset_TMDS::SetMode()
{
    RC rc;

    // SetMode on the display with the given windows
    CHECK_RC(m_pLwDisplay->SetMode(m_pDisplayPanel->displayId,
        m_pDisplayPanel->resolution.width,
        m_pDisplayPanel->resolution.height,
        m_pDisplayPanel->resolution.depth,
        m_pDisplayPanel->resolution.refreshrate,
        m_pDisplayPanel->head));

    m_pDisplayPanel->bActiveModeset  = true;

    return rc;
}

Modeset_HDMI::Modeset_HDMI(DisplayPanel *pDisplayPanel, Display *pDisplay): Modeset_TMDS(pDisplayPanel, pDisplay)
{
    m_panelType = LwDispPanelMgr::LWD_PANEL_TYPE_HDMI;
}

RC Modeset_HDMI::Initialize()
{
    RC rc;
    LW0073_CTRL_SPECIFIC_GET_I2C_PORTID_PARAMS i2cPortParams = {0};
    CHECK_RC(m_pDisplay->GetDisplaySubdeviceInstance(&(i2cPortParams.subDeviceInstance)));
    i2cPortParams.displayId = (UINT32)m_pDisplayPanel->displayId;

    CHECK_RC(m_pDisplay->RmControl(LW0073_CTRL_CMD_SPECIFIC_GET_I2C_PORTID,
        &i2cPortParams, sizeof(i2cPortParams)));

    UINT32 clkDtflags = 0;

    m_SupportsHdmi2 =
        (ReadWriteI2cRegister(
            m_pDisplay->GetOwningDisplaySubdevice(),
            i2cPortParams.ddcPortId -1,
            LW_HDMI_SCDC_SLAVE_ADR,
            LW_HDMI_SCDC_STATUS_FLAGS_0,
            &clkDtflags) == OK) ? true : false;

    return OK;
}

RC Modeset_HDMI::SetMode()
{
    RC rc;

    // Force enable HDMI infoframes
    m_pDisplay->ForceHdmiInfoFrames(true);

    if (m_SupportsHdmi2)
    {
        UINT08  rawEdid[EDID_SIZE_MAX];
        UINT32  rawEdidSize = EDID_SIZE_MAX;
        LWT_EDID_INFO   edidInfo;

        Edid    *pEdid = m_pDisplay->GetEdidMember();

        //retrieving the edid.
        CHECK_RC(pEdid->GetRawEdid(m_pDisplayPanel->displayId, rawEdid, &rawEdidSize));

        // lets grab the LWT_EDID_INFO
        memset(&edidInfo, 0, sizeof(edidInfo));
        LwTiming_ParseEDIDInfo(rawEdid, rawEdidSize, &edidInfo);

        // printing the HDMI ver2 params from the EDID.
        Printf(Tee::PriDebug, "edidInfo.hdmiForumInfo.scdc_present =%d\n", edidInfo.hdmiForumInfo.scdc_present);
        Printf(Tee::PriDebug, "edidInfo.hdmiForumInfo.rr_capable =%d\n", edidInfo.hdmiForumInfo.rr_capable);
        Printf(Tee::PriDebug, "edidInfo.hdmiForumInfo.lte_340Mcsc_scramble =%d\n", edidInfo.hdmiForumInfo.lte_340Mcsc_scramble);
        Printf(Tee::PriDebug, "edidInfo.hdmiForumInfo.max_TMDS_char_rate  =%d\n", edidInfo.hdmiForumInfo.max_TMDS_char_rate);

        if (m_pDisplayPanel->resolution.pixelClockHz > 540000000)
        {
            CHECK_RC(m_pDisplay->SetHdmi2_0Caps(m_pDisplayPanel->displayId,
                true,
                true,
                true));
        }
    }

    // SetMode on the display with the given windows
    CHECK_RC(m_pLwDisplay->SetMode(m_pDisplayPanel->displayId,
        m_pDisplayPanel->resolution.width,
        m_pDisplayPanel->resolution.height,
        m_pDisplayPanel->resolution.depth,
        m_pDisplayPanel->resolution.refreshrate,
        m_pDisplayPanel->head));

    // TODO HDMI custom settings applied  here.
    m_pDisplayPanel->bActiveModeset  = true;

    return rc;
}

RC Modeset_HDMI::Detach()
{
    return Modeset_TMDS::Detach();
}

Modeset_HDMI_FRL::Modeset_HDMI_FRL(DisplayPanel *pDisplayPanel, Display *pDisplay): Modeset_TMDS(pDisplayPanel, pDisplay)
{
    m_panelType = LwDispPanelMgr::LWD_PANEL_TYPE_HDMI_FRL;
}

RC Modeset_HDMI_FRL::Initialize()
{
    RC rc;
    LW0073_CTRL_SPECIFIC_GET_I2C_PORTID_PARAMS i2cPortParams = {0};
    CHECK_RC(m_pDisplay->GetDisplaySubdeviceInstance(&(i2cPortParams.subDeviceInstance)));
    i2cPortParams.displayId = (UINT32)m_pDisplayPanel->displayId;

    CHECK_RC(m_pDisplay->RmControl(LW0073_CTRL_CMD_SPECIFIC_GET_I2C_PORTID,
        &i2cPortParams, sizeof(i2cPortParams)));

    m_SupportsHdmiFrl = true;
    if (m_SupportsHdmiFrl)
    {
        UINT08  rawEdid[EDID_SIZE_MAX];
        UINT32  rawEdidSize = EDID_SIZE_MAX;
        LWT_EDID_INFO   edidInfo;

        Edid    *pEdid = m_pDisplay->GetEdidMember();

        // Retrieving the EDID.
        CHECK_RC(pEdid->GetRawEdid(m_pDisplayPanel->displayId, rawEdid, &rawEdidSize));

        // Lets grab the LWT_EDID_INFO
        memset(&edidInfo, 0, sizeof(edidInfo));
        LwTiming_ParseEDIDInfo(rawEdid, rawEdidSize, &edidInfo);

        // Printing the HDMI ver2.1 params from the EDID.
        Printf(Tee::PriDebug, "edidInfo.hdmiForumInfo.max_FRL_Rate =%d\n", edidInfo.hdmiForumInfo.max_FRL_Rate);
        Printf(Tee::PriDebug, "edidInfo.hdmiForumInfo.dsc_1p2 =%d\n", edidInfo.hdmiForumInfo.dsc_1p2);
        Printf(Tee::PriDebug, "edidInfo.hdmiForumInfo.dsc_Max_FRL_Rate =%d\n", edidInfo.hdmiForumInfo.dsc_Max_FRL_Rate);

        // Set the GPU HW limitation for HDMI FRL here.
        CHECK_RC(m_pDisplay->SetHdmiFrlCaps(m_pDisplayPanel->displayId,
                                            true,
                                            true,
                                            Display::FrlRate_4Lanes12G,
                                            Display::FrlRate_4Lanes12G));
    }

    return OK;
}

RC Modeset_HDMI_FRL::SetMode()
{
    RC rc;
    LwDispHeadSettings *pHeadSettings = NULL;
    LwDispORSettings *pORSettings = NULL;
    UINT32 orIndex = m_pDisplayPanel->sor;
    UINT32 headIndex = m_pDisplayPanel->head;

    // Set SOR settings for HDMI FRL
    pORSettings = &(m_pCoreChCtx->DispLwstomSettings.SorSettings[orIndex]);
    pORSettings->Dirty = true;
    pORSettings->ORNumber = orIndex;
    pORSettings->OwnerMask = 1 << headIndex;
    pORSettings->protocol = LwDispSORSettings::Protocol::PROTOCOL_HDMI_FRL;
    pORSettings->DevInfoProtocol = 0;
    pORSettings->CrcActiveRasterOnly = true;
    pHeadSettings = &(m_pCoreChCtx->DispLwstomSettings.HeadSettings[headIndex]);
    pHeadSettings->pORSettings = pORSettings;
    pHeadSettings->display = m_pDisplayPanel->displayId;

    // Force enable HDMI infoframes
    m_pDisplay->ForceHdmiInfoFrames(true);

    // SetMode on the display with the given windows
    CHECK_RC(m_pLwDisplay->SetMode(m_pDisplayPanel->displayId,
             m_pDisplayPanel->resolution.width,
             m_pDisplayPanel->resolution.height,
             m_pDisplayPanel->resolution.depth,
             m_pDisplayPanel->resolution.refreshrate,
             m_pDisplayPanel->head));

    // TODO: HDMI custom settings applied  here.
    m_pDisplayPanel->bActiveModeset  = true;

    return rc;
}

RC Modeset_HDMI_FRL::SetupLinkConfig(HDMI_FRL_LINK_CONFIG_DATA frlLinkConfig)
{
    RC rc;

    switch (frlLinkConfig.linkRate)
    {
        case 0:
        case 3:
        case 6:
        case 8:
        case 10:
        case 12:
            m_frlLinkConfig.linkRate = frlLinkConfig.linkRate;
            break;
        default:
            return RC::BAD_PARAMETER;
    }

    switch (frlLinkConfig.laneCount)
    {
        case 0:
        case 3:
        case 4:
            m_frlLinkConfig.laneCount = frlLinkConfig.laneCount;
            break;
        default:
            return RC::BAD_PARAMETER;
    }

    return rc;
}

RC Modeset_HDMI_FRL::DoFrlLinkTraining()
{
    RC rc;
    LW0073_CTRL_SPECIFIC_SET_HDMI_FRL_LINK_CONFIG_PARAMS frlLinkConfigParams = {0};
    UINT32 linkRate = m_frlLinkConfig.linkRate;
    UINT32 laneCount = m_frlLinkConfig.laneCount;
    UINT32 data = 0;

    CHECK_RC(m_pDisplay->GetDisplaySubdeviceInstance(&(frlLinkConfigParams.subDeviceInstance)));
    frlLinkConfigParams.displayId = (UINT32)m_pDisplayPanel->displayId;

    if ((laneCount == 0) || (linkRate == 0))
        data = DRF_DEF(0073, _CTRL_HDMI_FRL_DATA_SET, _FRL_RATE, _NONE);

    if (laneCount == 3)
    {
        switch (linkRate)
        {
            case 3:
                data = DRF_DEF(0073, _CTRL_HDMI_FRL_DATA_SET, _FRL_RATE, _3LANES_3G);
                break;
            case 6:
                data = DRF_DEF(0073, _CTRL_HDMI_FRL_DATA_SET, _FRL_RATE, _3LANES_6G);
                break;
            default:
                Printf(Tee::PriDebug, "linkRate=%d is not supported for 3 lanes mode.\n", linkRate);
                break;
        }
    }
    else if (laneCount == 4)
    {
        switch (linkRate)
        {
            case 6:
                data = DRF_DEF(0073, _CTRL_HDMI_FRL_DATA_SET, _FRL_RATE, _4LANES_6G);
                break;
            case 8:
                data = DRF_DEF(0073, _CTRL_HDMI_FRL_DATA_SET, _FRL_RATE, _4LANES_8G);
                break;
            case 10:
                data = DRF_DEF(0073, _CTRL_HDMI_FRL_DATA_SET, _FRL_RATE, _4LANES_10G);
                break;
            case 12:
                data = DRF_DEF(0073, _CTRL_HDMI_FRL_DATA_SET, _FRL_RATE, _4LANES_12G);
                break;
            default:
                Printf(Tee::PriDebug, "linkRate=%d is not supported for 4 lanes mode.\n", linkRate);
                break;
        }
    }
    else
    {
        Printf(Tee::PriDebug, "laneCount=%d is not supported.\n", laneCount);
    }
    frlLinkConfigParams.data = data;

    CHECK_RC(m_pDisplay->RmControl(LW0073_CTRL_CMD_SPECIFIC_SET_HDMI_FRL_CONFIG,
             &frlLinkConfigParams, sizeof(frlLinkConfigParams)));

    return rc;
}

RC Modeset_HDMI_FRL::Detach()
{
    return Modeset_TMDS::Detach();
}

Modeset_DSI::Modeset_DSI(DisplayPanel *pDisplayPanel, Display *pDisplay)
    : Modeset(pDisplayPanel, pDisplay)
{
    m_panelType = LwDispPanelMgr::LWD_PANEL_TYPE_DSI;
}

RC Modeset_DSI::Initialize()
{
    return OK;
}

RC Modeset_DSI::SetMode()
{
    RC rc;

    // SetMode on the display with the given windows
    CHECK_RC(m_pLwDisplay->SetMode(m_pDisplayPanel->displayId,
        m_pDisplayPanel->resolution.width,
        m_pDisplayPanel->resolution.height,
        m_pDisplayPanel->resolution.depth,
        m_pDisplayPanel->resolution.refreshrate,
        m_pDisplayPanel->head));

    m_pDisplayPanel->bActiveModeset  = true;

    return rc;
}

RC Modeset_DSI::Detach()
{
    RC rc;

    m_pLwDisplay->DetachDisplay(DisplayIDs(1, m_pDisplayPanel->displayId));

    m_pDisplayPanel->bActiveModeset  = false;

    return rc;
}

Modeset_DSI::~Modeset_DSI()
{
    // Detach devices here if needed
}

Modeset_DP::Modeset_DP(DisplayPanel *pDisplayPanel, Display *pDisplay)
    : Modeset(pDisplayPanel, pDisplay)
{
    RC rc;
    m_dpLinkConfig.linkRate = 0;
    m_dpLinkConfig.laneCount = 0;
    m_dpLinkConfig.bFec = false;

    if(pDisplay->IsMultiStreamDisplay(pDisplayPanel->displayId.Get()))
    {
        m_panelType = LwDispPanelMgr::LWD_PANEL_TYPE_DP_MST;
    }
    else
    {
        m_panelType = LwDispPanelMgr::LWD_PANEL_TYPE_DP;
    }
    m_pDpDispPanel = pDisplayPanel;

}

Modeset_DP::Modeset_DP
(
    pair<DisplayPanel *, DisplayPanel *> &dualSSTPair,
    Display *pDisplay
)
: Modeset(dualSSTPair.first, pDisplay)
{
    RC rc;
    m_dpLinkConfig.linkRate = 0;
    m_dpLinkConfig.laneCount = 0;
    m_dpLinkConfig.bFec = false;

    if(pDisplay->IsMultiStreamDisplay(dualSSTPair.first->displayId.Get()))
    {
        m_panelType = LwDispPanelMgr::LWD_PANEL_TYPE_DP_DUAL_MST;
    }
    else
    {
        m_panelType = LwDispPanelMgr::LWD_PANEL_TYPE_DP_DUAL_SST;
    }

    m_dualSSTPair = dualSSTPair;
    m_pDpDispPanel = dualSSTPair.first;
}

Modeset_DP::~Modeset_DP()
{
    RC rc;

    if (m_pDpDispPanel->isFakeDisplay)
    {
        if(m_pDpDispPanel->fakeDpProperties.pEdid)
        {
            free(m_pDpDispPanel->fakeDpProperties.pEdid);
        }
        rc = m_pDisplay->DisableAllSimulatedDPDevices(Display::SkipRealDisplayDetection, nullptr);
    }
}

RC Modeset_DP::Initialize()
{
    RC rc;

    if (GetPanelType() == LwDispPanelMgr::LWD_PANEL_TYPE_DP_DUAL_SST)
    {
        // configure SST mode for dualSST
        rc = m_pDisplay->SetSingleHeadMultiStreamMode
            (
                (m_dualSSTPair.first)->displayId,
                (m_dualSSTPair.second)->displayId,
                Display::SHMultiStreamSST
            );
        if ( rc != OK)
        {
            Printf(Tee::PriError,"\n %s failied to set SingleHeadMultiStreamMode", __FUNCTION__);
            return rc;
        }

        if ((m_dualSSTPair.first)->isFakeDisplay && (m_dualSSTPair.second)->isFakeDisplay)
        {
            UINT32 simulatedDisplayIDMask = 0;
            rc = m_pDisplay->EnableSimulatedDPDevices
                (
                    (m_dualSSTPair.first)->displayId,
                    (m_dualSSTPair.second)->displayId,
                    Display::DPMultistreamDisabled,
                    1,
                    m_pDisplayPanel->fakeDpProperties.pEdid,
                    m_pDisplayPanel->fakeDpProperties.edidSize,
                    &simulatedDisplayIDMask
                );
            if (rc != OK)
            {
                Printf(Tee::PriError,"\n %s failed to simulate 2-SST Device", __FUNCTION__);
                return rc;
            }
            CHECK_RC(m_pDisplay->SetBlockHotPlugEvents(true));
        }
    }

    else if (GetPanelType() == LwDispPanelMgr::LWD_PANEL_TYPE_DP_DUAL_MST)
    {
        rc = m_pDisplay->SetSingleHeadMultiStreamMode
                (
                    (m_dualSSTPair.first)->displayId,
                    (m_dualSSTPair.second)->displayId,
                    Display::SHMultiStreamMST
                );

        if ( rc != OK)
        {
            Printf(Tee::PriError,"\n %s failied to set SingleHeadMultiStreamMode", __FUNCTION__);
            return rc;
        }
    }
    else
    {
        if (m_pDisplayPanel->isFakeDisplay && m_panelType == LwDispPanelMgr::LWD_PANEL_TYPE_DP)
        {
            UINT32 simulatedDisplayIDMask = 0;
            rc = m_pDisplay->EnableSimulatedDPDevices(
                m_pDisplayPanel->displayId,
                Display::NULL_DISPLAY_ID,
                Display::DPMultistreamDisabled,
                1,
                m_pDisplayPanel->fakeDpProperties.pEdid,
                m_pDisplayPanel->fakeDpProperties.edidSize,
                &simulatedDisplayIDMask);
            if (rc != OK)
            {
                Printf(Tee::PriError,"\n %s failed to simulate Device", __FUNCTION__);
                return rc;
            }
            CHECK_RC(m_pDisplay->SetBlockHotPlugEvents(true));
        }
    }

    return rc;
}

RC Modeset_DP::SendSDP(Head head, DisplayId displayId)
{
    RC rc;
    LwU32 DisplaySubdevice;
    CHECK_RC(m_pDisplay->GetDisplaySubdeviceInstance(&DisplaySubdevice));

    LW0073_CTRL_SPECIFIC_SET_OD_PACKET_PARAMS PacketParams = {0};
    memset(&PacketParams, 0, sizeof(PacketParams));

    PacketParams.subDeviceInstance = DisplaySubdevice;
    PacketParams.displayId = displayId;

    PacketParams.transmitControl =
        DRF_DEF(0073, _CTRL_SPECIFIC_SET_OD_PACKET_TRANSMIT_CONTROL,
            _SINGLE_FRAME, _DISABLE) |
        DRF_DEF(0073, _CTRL_SPECIFIC_SET_OD_PACKET_TRANSMIT_CONTROL,
            _OTHER_FRAME, _DISABLE) |
        DRF_DEF(0073, _CTRL_SPECIFIC_SET_OD_PACKET_TRANSMIT_CONTROL,
            _ENABLE, _YES) |
        DRF_DEF(0073, _CTRL_SPECIFIC_SET_OD_PACKET_TRANSMIT_CONTROL,
            _ON_HBLANK, _DISABLE);

    PacketParams.packetSize = sizeof(LWT_HDR_INFOFRAME_MASTERING_DATA) + 4 + 4; // 4 bytes for the header + 4 bytes for metadata
    // SDP Header for HDR static metadata
    PacketParams.aPacket[0] = 0;
    PacketParams.aPacket[1] = dp_pktType_DynamicRangeMasteringInfoFrame;
    PacketParams.aPacket[2] = DP_INFOFRAME_SDP_V1_3_NON_AUDIO_SIZE - 1;
    PacketParams.aPacket[3] = DP_INFOFRAME_SDP_V1_3_VERSION << DP_INFOFRAME_SDP_V1_3_HB3_VERSION_SHIFT;

    // Payload
    PacketParams.aPacket[4] = 0x01;
    PacketParams.aPacket[5] = 0x1a;
    PacketParams.aPacket[6] = 0x02;
    PacketParams.aPacket[7] = 0x00;
    PacketParams.aPacket[8] = ((UINT16)(chromacityList[m_color].red_x/0.00002)) & 0xFF; 
    PacketParams.aPacket[9] = ((((UINT16)(chromacityList[m_color].red_x/0.00002)) & 0xFF00)>>8);
    PacketParams.aPacket[10] = ((UINT16)(chromacityList[m_color].red_y/0.00002)) & 0xFF;
    PacketParams.aPacket[11] = ((((UINT16)(chromacityList[m_color].red_y/0.00002)) & 0xFF00)>>8);
    PacketParams.aPacket[12] = ((UINT16)(chromacityList[m_color].green_x/0.00002)) & 0xFF;
    PacketParams.aPacket[13] = ((((UINT16)(chromacityList[m_color].green_x/0.00002)) & 0xFF00)>>8);
    PacketParams.aPacket[14] = ((UINT16)(chromacityList[m_color].green_y/0.00002)) & 0xFF;
    PacketParams.aPacket[15] = ((((UINT16)(chromacityList[m_color].green_y/0.00002)) & 0xFF00)>>8);
    PacketParams.aPacket[16] = ((UINT16)(chromacityList[m_color].blue_x/0.00002)) & 0xFF;
    PacketParams.aPacket[17] = ((((UINT16)(chromacityList[m_color].blue_x/0.00002)) & 0xFF00)>>8);
    PacketParams.aPacket[18] = ((UINT16)(chromacityList[m_color].blue_y/0.00002)) & 0xFF;
    PacketParams.aPacket[19] = ((((UINT16)(chromacityList[m_color].blue_y/0.00002)) & 0xFF00)>>8);
    PacketParams.aPacket[20] = ((UINT16)(chromacityList[m_color].wp_x/0.00002)) & 0xFF;
    PacketParams.aPacket[21] = ((((UINT16)(chromacityList[m_color].wp_x/0.00002)) & 0xFF00)>>8);
    PacketParams.aPacket[22] = ((UINT16)(chromacityList[m_color].wp_y/0.00002)) & 0xFF;
    PacketParams.aPacket[23] = ((((UINT16)(chromacityList[m_color].wp_y/0.00002)) & 0xFF00)>>8);
    PacketParams.aPacket[24] = 0xe8;
    PacketParams.aPacket[25] = 0x03;
    PacketParams.aPacket[26] = 0x2c;
    PacketParams.aPacket[27] = 0x01;
    PacketParams.aPacket[28] = 0xe8;
    PacketParams.aPacket[29] = 0x03;
    PacketParams.aPacket[30] = 0x14;
    PacketParams.aPacket[31] = 0;

    CHECK_RC(m_pLwDisplay->RmControl(LW0073_CTRL_CMD_SPECIFIC_SET_OD_PACKET,
                       &PacketParams, sizeof(PacketParams)));

    return rc;
}

RC Modeset_DP::SetMode()
{
    RC rc;

    // Do modeset on primary Display Id.
    CHECK_RC(m_pLwDisplay->SetMode(m_pDisplayPanel->displayId,
        m_pDisplayPanel->resolution.width,
        m_pDisplayPanel->resolution.height,
        m_pDisplayPanel->resolution.depth,
        m_pDisplayPanel->resolution.refreshrate,
        m_pDisplayPanel->head));

    m_pDisplayPanel->bActiveModeset  = true;

    if ((getDispMode() == ENABLE_HDR_SETTINGS ||
         getDispMode() == ENABLE_SDR_SETTINGS) &&
         !m_pDisplay->GetOwningDisplaySubdevice()->IsEmOrSim())
    {
        SendSDP(m_pDisplayPanel->head, m_pDisplayPanel->displayId);
    }

    return rc;
}

RC Modeset_DP::Detach()
{
   RC rc;

   m_pLwDisplay->DetachDisplay(DisplayIDs(1, m_pDisplayPanel->displayId));

   m_pDisplayPanel->bActiveModeset  = false;

   return rc;
}

RC Modeset_TMDS::Initialize()
{
    return OK;
}

RC Modeset_TMDS::Detach()
{
    RC rc;

    CHECK_RC(m_pDisplay->DetachDisplay(DisplayIDs(1, m_pDisplayPanel->displayId)));

    m_pDisplayPanel->bActiveModeset  = false;

    return rc;
}

//! ReadWriteI2cRegister
//!     Reads & Writes Display's DDC registers
//! dispId       : display whose register is to be read.
//! I2CAddress   : I2CAddress of the device.
//! I2CRegisterOFfset : offset of register to be read.
//! pReadData    : pointer specifying data to be written or stores data read from register.
//! bReadWrite   : false = read; true = write
RC Modeset_TMDS::ReadWriteI2cRegister(GpuSubdevice *pSubdev, LwU32 I2CPort,
                                      UINT32 I2CAddress,
                                      UINT32 I2CRegisterOFfset,
                                      UINT32 *pReadData,
                                      bool bReadWrite)

{
    RC rc;

    if (!pSubdev->SupportsInterface<I2c>())
    {
        Printf(Tee::PriError, "%s does not support I2C\n", pSubdev->GetName().c_str());
        return RC::UNSUPPORTED_DEVICE;
    }

    I2c::Dev i2cDev = pSubdev->GetInterface<I2c>()->I2cDev(I2CPort, I2CAddress);

    if (bReadWrite)
    {
        rc = i2cDev.Write(I2CRegisterOFfset, *pReadData);
    }
    else
    {
        *pReadData = 0;
        rc = i2cDev.Read(I2CRegisterOFfset, pReadData);
    }

    Printf(Tee::PriHigh,
               "%s: I2C Operation:%s, [DDCPort] = %d, [I2CAddress] = %d, [I2CRegisterOFfset] = %d, Data = %0x, RC= %d\n",
                __FUNCTION__, bReadWrite ? "Write" : "Read",  I2CPort, I2CAddress, I2CRegisterOFfset,
                *pReadData, (UINT32)rc);

    return rc;
}

RC Modeset::VerifyHDCP()
{
    RC rc;
    HDCPUtils hdcpUtils;

    CHECK_RC(hdcpUtils.Init(m_pDisplay, m_pDisplayPanel->displayId));

    Tasker::Sleep(5000);

    CHECK_RC(hdcpUtils.PerformJob(HDCPUtils::RENEGOTIATE_LINK));

    // For DP Dongle sleep time should be 30s
    Tasker::Sleep(hdcpUtils.GetSleepAfterRenegotiateTime());

    bool            isHdcp1xEnabled = false;
    bool            isGpuCapable = false;
    bool            isHdcp22Capable = false;
    bool            isHdcp22Enabled = false;
    bool            isRepeater = false;
    bool            gethdcpStatusCached = true;

    rc = hdcpUtils.GetHDCPInfo(m_pDisplayPanel->displayId, gethdcpStatusCached, isHdcp1xEnabled, isGpuCapable,
                               isGpuCapable, isHdcp22Capable, isHdcp22Enabled, isRepeater);

    if (FLCN_OK == rc)
    {
        Printf(Tee::PriHigh, "Succeed to GetHDCPInfo: isGpuCapable:%d isHdcp1xEnabled:%d, isHdcp22Capable:%d isHdcp22Enabled:%d\n",
               isGpuCapable,
               isHdcp1xEnabled,
               isHdcp22Capable,
               isHdcp1xEnabled);
    }
    else
    {
        Printf(Tee::PriHigh, "Failed to Get HDCPInfo\n");
        return rc;
    }

    if (isHdcp22Enabled || isHdcp1xEnabled)
    {

        Printf(Tee::PriHigh, "Hdcp Downstream succeeded\n");

        // Check for upstream
        CHECK_RC(hdcpUtils.PerformJob(HDCPUtils::READ_LINK_STATUS));
    }
    else
    {
        Printf(Tee::PriHigh, "Hdcp Downstream failed\n");

        rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

RC Modeset::VerifyDisp(bool manualVerif,
                       string crcFileName,
                       string goldenFileDir,
                       bool bGenerateGold)
{
    RC rc;

    if (manualVerif)
    {
        SimpleUserInterface * pInterface = new SimpleUserInterface(true);
        Printf(Tee::PriHigh,
            " Type YES if you see image correctly \n");
        string inputNumStr = pInterface->GetLine();
        if (inputNumStr.find("YES") != string::npos)
        {
            rc = OK;
        }
        else
        {
            rc = RC::SOFTWARE_ERROR;
        }
        delete pInterface;
    }
    else
    {
        rc = AutoVerification(crcFileName,
                              goldenFileDir,
                              bGenerateGold);
        if (OK != rc)
        {
            Printf(Tee::PriHigh, "\n %s => AutoVerification failed with RC = %d \n",
                __FUNCTION__, (UINT32)rc);
            if (rc == RC::FILE_DOES_NOT_EXIST)
            {
                if (!bGenerateGold)
                {
                    Printf(Tee::PriHigh, "\n %s => AutoVerification failed with RC = %d \n",
                        __FUNCTION__, (UINT32)rc);

                }
                else
                {
                    rc = OK;
                }
            }
            else
            {
                return rc;
            }
        }
    }

    return rc;
}

RC Modeset::AutoVerification(bool generateGold)
{
    return AutoVerification(m_pDisplayPanel->m_crcFileName,
                            m_pDisplayPanel->m_goldenCrcDir,
                            generateGold);
}

//! AutoVerification
//! automatically verfies the output by comparing CRCs
//----------------------------------------------------------------------
RC Modeset::AutoVerification(string crcFileName,
                             string goldenFolderDir,
                             bool generateGold)
{
    RC rc;
    CrcComparison crcCompObj;

    bool bCrcCompFlagAlloced = false;
    vector<string> searchPath;

    searchPath.push_back (".");
    Utility::AppendElwSearchPaths(&searchPath,"LD_LIBRARY_PATH");
    Utility::AppendElwSearchPaths(&searchPath,"MODS_RUNSPACE");

    m_pDisplayPanel->m_crcFileName = crcFileName;
    m_pDisplayPanel->m_goldenCrcDir = goldenFolderDir;

    VerifyCrcTree   *pCrcCompFlag = new VerifyCrcTree();
    pCrcCompFlag->crcHeaderField.bSkipChip = pCrcCompFlag->crcHeaderField.bSkipPlatform = true;
    bCrcCompFlagAlloced = true;

    m_crcSettings.NumExpectedCRCs  = 1;
    UINT32 compositorCrc = 0, primaryCrc = 0, secondaryCrc = 0;

    CHECK_RC(m_pLwDisplay->GetCrcValues(
                                    &m_crcSettings,
                                    &compositorCrc,
                                    &primaryCrc,
                                    &secondaryCrc,
                                    m_pDisplayPanel->head));

    CHECK_RC(crcCompObj.DumpCrcToXml(m_pDisplay->GetOwningDevice(),
                                     crcFileName.c_str(),
                                     &m_crcSettings,
                                     true));

    if( !CrcComparison::FileExists(crcFileName.c_str(), &searchPath))
    {
        Printf(Tee::PriHigh, "\n ****************************ERROR******************************");
        Printf(Tee::PriHigh, "\n %s => CRC Current Run File [%s] was not created. Returning RC:: FILE_DOES_NOT_EXIST \n",
            __FUNCTION__, crcFileName.c_str());
        Printf(Tee::PriHigh, "\n **********************************************************************");
        return RC::FILE_DOES_NOT_EXIST;
    }

    string logFileName = crcFileName + string(".log");
    string goldenFileName = string(goldenFolderDir + crcFileName);

    if( !CrcComparison::FileExists(goldenFileName.c_str(), &searchPath))
    {
        Printf(Tee::PriHigh, "\n ************** ERROR::CRC GOLDEN FILE DOES NOT EXIST **************");
        Printf(Tee::PriHigh, "\n %s => CRC Golden File [%s] does not exists. Returning RC:: FILE_DOES_NOT_EXIST \n",
            __FUNCTION__,goldenFileName.c_str());
        Printf(Tee::PriHigh, "\n =>PLEASE SUBMIT GOLDEN FILE FOR THIS CONFIG TO PERFORCE PATH");
        Printf(Tee::PriHigh, "\n **********************************************************************");
        return RC::FILE_DOES_NOT_EXIST;
    }

    //
    // Since we use diffrent sors and all sors has same capabilities,
    // its safe to skip primary CRC sor number.
    pCrcCompFlag->crcNotifierField.bSkipPrimaryCrcOutput = true;

    // API to take diff between 2 XML crc files
    rc = crcCompObj.CompareCrcFiles(crcFileName.c_str(),
                                    goldenFileName.c_str(),
                                    logFileName.c_str(),
                                    pCrcCompFlag);
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "\n %s => CRC Compare failed for Golden File [%s]. RC = %d \n",
            __FUNCTION__, goldenFileName.c_str(), (UINT32)rc);
    }

    if (bCrcCompFlagAlloced)
    {
        delete pCrcCompFlag;
    }

    return rc;
}

RC Modeset_DP::DoDpLinkTraining()
{
    RC rc;

    if(!m_dpLinkConfig.laneCount)
    {
        return OK;
    }

    bool force = false;

    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        force = true;
    }

    //Reset old setting
    CHECK_RC(m_pDisplay->GetDisplayPortMgr()->SetLinkConfigPolicy(m_pDisplayPanel->displayId,
        FALSE, // commit
        TRUE,  // force
        FALSE, // m_mstMode
        DisplayPortMgr::DP_LINK_CONFIG_POLICY_DEFAULT,
        0,     // LinkRate = 0
        laneCount_0));

    //Set new config
    CHECK_RC(m_pDisplay->GetDisplayPortMgr()->SetLinkConfigPolicy(m_pDisplayPanel->displayId,
             FALSE, // commit
             force,  // force
             FALSE, // m_mstMode
             DisplayPortMgr::DP_LINK_CONFIG_POLICY_PREFERRED,
             m_dpLinkConfig));

    return rc;
}

RC Modeset_DP::SetupLinkConfig(DisplayPortMgr::DP_LINK_CONFIG_DATA dpLinkConfig)
{
    RC rc;

    if (dpLinkConfig.linkRate == 0)
    {
        CHECK_RC(m_pDisplay->GetDisplayPortMgr()->GetMaxLinkRateAssessed(m_pDisplayPanel->displayId.Get(), &dpLinkConfig.linkRate));
    }
    else if (dpLinkConfig.linkRate != DisplayPort::RBR &&
             dpLinkConfig.linkRate != DisplayPort::HBR &&
             dpLinkConfig.linkRate != DisplayPort::HBR2 &&
             dpLinkConfig.linkRate != DisplayPort::HBR3)
    {
        Printf(Tee::PriError,
            "%s Invalid DP Link Rate \n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    if (dpLinkConfig.laneCount == 0)
    {
        dpLinkConfig.laneCount = laneCount_4;
    }
    else if (dpLinkConfig.laneCount != laneCount_1 &&
             dpLinkConfig.laneCount != laneCount_2 &&
             dpLinkConfig.laneCount != laneCount_4)
    {

        Printf(Tee::PriError,
            "%s Invalid DP Lane Count \n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    m_dpLinkConfig.linkRate = dpLinkConfig.linkRate;
    m_dpLinkConfig.laneCount = dpLinkConfig.laneCount;
    m_dpLinkConfig.bFec = dpLinkConfig.bFec;

    return rc;
}

bool Modeset::IsPanelTypeDP()
{
    return ((m_panelType == LwDispPanelMgr::LWD_PANEL_TYPE_DP) ||
            (m_panelType == LwDispPanelMgr::LWD_PANEL_TYPE_DP_MST) ||
            (m_panelType == LwDispPanelMgr::LWD_PANEL_TYPE_DP_DUAL_SST) ||
            (m_panelType == LwDispPanelMgr::LWD_PANEL_TYPE_DP_DUAL_MST) ||
            (m_panelType == LwDispPanelMgr::LWD_PANEL_TYPE_EDP));
}

bool Modeset::IsPanelTypeHDMIFRL()
{
    return (m_panelType == LwDispPanelMgr::LWD_PANEL_TYPE_HDMI_FRL);
}



