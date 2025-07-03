/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2012-2013,2019 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

#include "gpu/tests/rmtest.h"
#include "core/include/display.h"
#include "core/include/utility.h"
#include "gpu/tests/rm/utility/crccomparison.h"
#include "core/include/modsedid.h"
#include <string>
#include "core/include/platform.h"
#include "rmt_fermipstateswitch.h"
#include "gpu/tests/rm/dispstatecheck/dispstate.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/display/evo_disp.h"
#include "gpu/tests/rm/memsys/rmt_acrverify.h"
#include "rmlsfm.h"

/*!
 * @file: rmt_pstatewithmodeset.cpp
 *
 * Implements a rmtest to switch pstates with modesets.
 *
 * Loops through available resolution for a given display and after
 * each modeset, loops through available pstates. If the chip is
 * displayless, this test does only pstate switches
 */

class PstateWithModeset:public RmTest
{
public:
    PstateWithModeset();
    virtual ~PstateWithModeset();
    virtual string  IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    RC    SetupPstatesInfo();
    RC    ProgramPstate(LwU32, LwU32);
    RC    ProgramPstateAndCompareCRC(LwU32, LwU32);
    RC    RunBasicPstateWithModesetTest();
    RC    GetDisplayResolutions();
    RC    GetCRCValuesAndDump(DisplayID, EvoCRCSettings::CHANNEL,
                                  LwU32&, LwU32&, string, bool);
    RC    SwitchPstates(LwU32, bool);
    RC    AllocSetLUTAndGamma(DisplayID Display,
                               EvoLutSettings *&pLutSettings);
    RC    FreeLUT(DisplayID Display,
              EvoLutSettings *pLutSettings);
    RC    GetLwrrentPstate(LwU32&, LwU32);
    LwU32 GetLogBase2(LwU32);

    SETGET_PROP(loopCount, LwU32);
    SETGET_PROP(maxWidth, LwU32);
    SETGET_PROP(maxHeight, LwU32);
    SETGET_PROP(maxRefreshRate, LwU32);
    SETGET_PROP(depth, LwU32);
    SETGET_PROP(allModes, LwU32);
    SETGET_PROP(edidFile, string);
    SETGET_PROP(minWidth, LwU32);
    SETGET_PROP(minHeight, LwU32);
    SETGET_PROP(minRefreshRate, LwU32);
    SETGET_PROP(maxPstateSwitches, LwU32);

private:
    LwRmPtr      pLwRm;
    LwRm::Handle m_hBasicClient;
    LwRm::Handle m_hBasicSubDev;
    GpuSubdevice *m_pSubdevice;
    Display      *m_pDisplay;
    DisplayID    selectedDisplay;
    bool         m_bIsDisplayAvailable;

    //! P-States parameter structures for RM ctrl cmds
    LW2080_CTRL_PERF_GET_PSTATES_INFO_PARAMS m_getAllPStatesInfoParams;
    LW2080_CTRL_PERF_GET_LWRRENT_PSTATE_PARAMS m_getLwrrentPStateParams;
    LW2080_CTRL_PERF_SET_FORCE_PSTATE_PARAMS m_setForcePStateParams;
    vector<LwU32> m_vPStateMasks;

    //Modeset details
    LwU32   *m_widths;
    LwU32   *m_heights;
    LwU32   *m_refreshRates;
    LwU32   m_totalResCount;

    LwU32   m_loopCount;
    LwU32   m_maxWidth;
    LwU32   m_maxHeight;
    LwU32   m_maxRefreshRate;
    LwU32   m_depth;
    LwU32   m_allModes;
    LwU32   m_forceLwstomEdid;
    string  m_edidFile;
    LwU32   m_minWidth;
    LwU32   m_minHeight;
    LwU32   m_minRefreshRate;
    LwU32   m_maxPstateSwitches;
    LwU32   m_lwrrentPstateSwitches;
};

/*
 * Set default values for modeset variables.
 */

PstateWithModeset::PstateWithModeset()
{
    SetName("PstateWithModeset");
    m_totalResCount = 0;
    selectedDisplay = 0;
    m_loopCount     = 1;

    m_maxWidth      = 0xFFFF;
    m_maxHeight     = 0xFFFF;
    m_maxRefreshRate= 0xFF;

    m_depth         = 32;
    m_allModes      = 0; //Don't loop through all resolutions. Use max width, height and refreshrate to restrict.
    m_forceLwstomEdid = 0;
    m_minWidth      = 0;
    m_minHeight     = 0;
    m_minRefreshRate= 0;
    m_maxPstateSwitches = 2;
}

PstateWithModeset::~PstateWithModeset()
{

}

//Gets all resolutions for selectedDisplay
RC PstateWithModeset::GetDisplayResolutions()
{
    RC rc;
    Edid *pEdid = m_pDisplay->GetEdidMember();
    if(!selectedDisplay)
    {
        Printf(Tee::PriHigh, "Selected display not found\n");
        return RC::SOFTWARE_ERROR;
    }
    CHECK_RC(pEdid->GetNumListedResolutions(selectedDisplay, (UINT32*)&m_totalResCount));
    if(!m_totalResCount)
    {
        Printf(Tee::PriHigh, "No resolutions found\n");
        return RC::SOFTWARE_ERROR;
    }
    m_widths       = new LwU32[m_totalResCount];
    m_heights      = new LwU32[m_totalResCount];
    m_refreshRates = new LwU32[m_totalResCount];

    CHECK_RC(pEdid->GetListedResolutions(selectedDisplay, (UINT32*)m_widths, (UINT32*)m_heights, (UINT32*)m_refreshRates));
    return OK;
}

string PstateWithModeset::IsTestSupported()
{
    DisplayIDs           Detected;
    LwU32                index     = 0;
    Display::ClassFamily dispClass = Display::NULLDISP;

    m_pSubdevice = GetBoundGpuSubdevice();
    m_pDisplay = GetDisplay();
    if (m_pDisplay)
        dispClass = m_pDisplay->GetDisplayClassFamily();

    m_bIsDisplayAvailable = FALSE;
    if (dispClass == Display::EVO)
    {
        m_bIsDisplayAvailable = TRUE;
        if (m_pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS) != OK)
            return "Display HW cannot be initialized";

        // get all the connected displays
        if (m_pDisplay->GetDetected(&Detected) != OK)
            return "No displays are detected";

        if (!Detected.size())
            return "No displays are detected";

        if (strcmp(m_edidFile.c_str(), ""))
        {
            Printf(Tee::PriHigh, "Forcing custom EDID file %s\n", m_edidFile.c_str());
            m_forceLwstomEdid = 1;
        }

        Printf(Tee::PriHigh, "Number of detected displays = %u\n", (LwU32)Detected.size());
        for (index = 0; index < Detected.size(); index++)
        {
            selectedDisplay = Detected[index];

            if (m_forceLwstomEdid)
            {
                if (DTIUtils::EDIDUtils::SetLwstomEdid(m_pDisplay, selectedDisplay, true, m_edidFile) != OK)
                {
                    selectedDisplay = 0;
                    continue;
                }
            }
            else if (! (DTIUtils::EDIDUtils::IsValidEdid(m_pDisplay, selectedDisplay) &&
                  DTIUtils::EDIDUtils::EdidHasResolutions(m_pDisplay, selectedDisplay))
               )
            {
                //
                // Custom EDID force or Display doesnt have a valid EDID or doesnt have any resolution in the EDID
                //
                if (DTIUtils::EDIDUtils::SetLwstomEdid(m_pDisplay, selectedDisplay, true) != OK)
                {
                    selectedDisplay = 0;
                    continue;
                }
            }
            break;
        }

        //
        // No display is found with valid EDID and setting lwstomEDID too failed on those displays.
        // Test not supported
        //
        if (!selectedDisplay)
        {
            return "Found NO Displays with valid EDID";
        }

    }

    return RUN_RMTEST_TRUE;
}

LwU32 PstateWithModeset::GetLogBase2(LwU32 nNum)
{
    return (LwU32)ceil(log((double)nNum)/log(2.0));
}

/* Gets the current Pstate
 */
RC PstateWithModeset::GetLwrrentPstate(LwU32 &lwrrPstate, LwU32 fallback)
{
    RC rc;
    switch (fallback)
    {
        case LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR:
        case LW2080_CTRL_PERF_PSTATE_FALLBACK_HIGHER_PERF:
        case LW2080_CTRL_PERF_PSTATE_FALLBACK_LOWER_PERF:
            break;
        default:
            Printf(Tee::PriHigh, "Unknown fall back strategy %d\n", (LwU32)(fallback));
            return RC::BAD_PARAMETER;
    }
    m_getLwrrentPStateParams.lwrrPstate = LW2080_CTRL_PERF_PSTATES_UNDEFINED;
    m_setForcePStateParams.fallback = fallback;
    CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                    LW2080_CTRL_CMD_PERF_GET_LWRRENT_PSTATE,
                    &m_getLwrrentPStateParams,
                    sizeof(m_getLwrrentPStateParams)));
    lwrrPstate = m_getLwrrentPStateParams.lwrrPstate;
    return OK;
}

/* Programs targetPstate and verifies if setting Pstate is successful or not.
 */
RC PstateWithModeset::ProgramPstate(LwU32 targetPstate, LwU32 fallback)
{
    RC rc;
    LwU32 lwrrPstate;

    if (targetPstate < LW2080_CTRL_PERF_PSTATES_P0 ||
        targetPstate > LW2080_CTRL_PERF_PSTATES_MAX)
    {
        Printf(Tee::PriHigh, "Unknown target P-State P%u (Mask 0x%08x)\n", GetLogBase2(targetPstate),
                           (LwU32)(targetPstate));
        return RC::BAD_PARAMETER;
    }

    switch (fallback)
    {
        case LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR:
        case LW2080_CTRL_PERF_PSTATE_FALLBACK_HIGHER_PERF:
        case LW2080_CTRL_PERF_PSTATE_FALLBACK_LOWER_PERF:
            break;
        default:
            Printf(Tee::PriHigh, "Unknown fall back strategy %d\n", (LwU32)(fallback));
            return RC::BAD_PARAMETER;
    }
    m_setForcePStateParams.forcePstate = targetPstate;
    m_setForcePStateParams.fallback = fallback;
    if ((rc = LwRmPtr()->ControlBySubdevice(m_pSubdevice, LW2080_CTRL_CMD_PERF_SET_FORCE_PSTATE,
                         &m_setForcePStateParams, sizeof(m_setForcePStateParams))) != OK)
    {
        Printf(Tee::PriHigh, "%d:Forced trasition to P-State P%u (Mask 0x%08x)failed\n", __LINE__,
                              GetLogBase2(targetPstate), (LwU32)(targetPstate));
        return rc;
    }

    // Verify whether the switch to the desired p-state actually took place
    CHECK_RC(GetLwrrentPstate(lwrrPstate, fallback));

    if (lwrrPstate != targetPstate)
    {
        Printf(Tee::PriHigh, "Setting to P-State P%u (Mask 0x%08x) failed !!\n",
              GetLogBase2(targetPstate), (LwU32)(targetPstate));

        Printf(Tee::PriHigh, "Target P-State : P%u (Mask 0x%08x)\n"
                     "Achieved P-State : P%u (Mask 0x%08x)\n",
                     GetLogBase2(targetPstate), (LwU32)(targetPstate),
                     GetLogBase2(lwrrPstate),
                     (LwU32)(lwrrPstate));

        if (LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR == fallback)
        {
            return RC::CANNOT_SET_STATE;
        }
    }
    else
    {
        Printf(Tee::PriHigh, "Setting to P-State P%u (Mask 0x%08x) successful !!\n",
               GetLogBase2(targetPstate), (LwU32)(targetPstate));
    }
    return OK;
}

RC PstateWithModeset::AllocSetLUTAndGamma(DisplayID Display,
                                      EvoLutSettings *&pLutSettings)
{
    RC rc;
    vector<LwU32>TestLut;

    LwU32 index = 0;

    pLutSettings = new EvoLutSettings();

    EvoDisplay *pEvoDisplay = GetDisplay()->GetEvoDisplay();

    //Set LUT Pallette
    for ( index = 0; index < 256; index++)
    {
        TestLut.push_back( ((LwU32)U8_TO_LUT(index)) << 16 | U8_TO_LUT(index) );  // Green, Red
        TestLut.push_back( (LwU32)U8_TO_LUT(index) );  // Blue
    }

    //257th value = 256th value
    TestLut.push_back( (((LwU32)U8_TO_LUT(index-1)) << 16) | U8_TO_LUT(index-1) );  // Green, Red
    TestLut.push_back( (LwU32)U8_TO_LUT(index-1) );  // Blue

    CHECK_RC(pEvoDisplay->SetupLUTBuffer(Display,
                                         pLutSettings,
                                         (UINT32*)&TestLut[0],
                                         (LwU32)TestLut.size()));

    //SetLUT After Base Is Active
    CHECK_RC(pEvoDisplay->SetLUT(Display,
                                 pLutSettings));

    // SET CUSTOM GAMMA
    // 0 -10 .. is the values
    CHECK_RC(pEvoDisplay->SetGamma(Display, pLutSettings, 2, 0, 0));

    return rc;
}

RC PstateWithModeset::FreeLUT(DisplayID Display,
                          EvoLutSettings *pLutSettings)
{
    RC rc;
    EvoLutSettings LutSettings;
    LutSettings.Dirty = false;
    LutSettings.pLutBuffer = NULL;
    CHECK_RC(GetDisplay()->GetEvoDisplay()->SetLUT(Display,
                                 &LutSettings));
    delete pLutSettings;
    pLutSettings = NULL;

    return rc;
}

// Gets OR crc values
RC PstateWithModeset::GetCRCValuesAndDump
(
    DisplayID               Display,
    EvoCRCSettings::CHANNEL CntlChannel,
    LwU32                   &CompCRC,
    LwU32                   &PriCRC,
    string                  CrcFileName,
    bool                    CreateNewFile
)
{
    RC              rc;
    CrcComparison   crcCompObj;
    LwU32          SecCRC;

    EvoDisplay *pEvoDisplay = GetDisplay()->GetEvoDisplay();

    EvoCRCSettings CrcStgs;
    CrcStgs.ControlChannel = CntlChannel;
    CrcStgs.CrcPrimaryOutput = EvoCRCSettings::CRC_OR_SF;

    CHECK_RC(pEvoDisplay->SetupCrcBuffer(Display, &CrcStgs));
    CHECK_RC(pEvoDisplay->GetCrcValues(Display, &CrcStgs, (UINT32*)&CompCRC, (UINT32*)&PriCRC, (UINT32*)&SecCRC));
    CHECK_RC(crcCompObj.DumpCrcToXml(GetDisplay()->GetOwningDevice(),
                                        CrcFileName.c_str(), &CrcStgs, CreateNewFile));

    return rc;
}

// Get CRC for OR before and after pstate switch. Compares them.
RC PstateWithModeset::ProgramPstateAndCompareCRC(LwU32 targetPstate, LwU32 fallback)
{
    RC rc;
    LwU32 coreORCrc[2], corePriCrc[2];

    if (m_bIsDisplayAvailable)
    {
        CHECK_RC(GetCRCValuesAndDump(selectedDisplay, EvoCRCSettings::CORE, coreORCrc[0],
                                                         corePriCrc[0], "coreCrc1.xml", true));
    }

    CHECK_RC(ProgramPstate(targetPstate, fallback));

    m_lwrrentPstateSwitches += 1;
    if (m_bIsDisplayAvailable)
    {
        CHECK_RC(GetCRCValuesAndDump(selectedDisplay, EvoCRCSettings::CORE, coreORCrc[1],
                                                         corePriCrc[1], "coreCrc2.xml", true));
        if(coreORCrc[0] != coreORCrc[1])
        {
            Printf(Tee::PriHigh, "OR CRC for Core didn't match after a pstate switch\n");
            return RC::GOLDEN_VALUE_MISCOMPARE;
        }

        if(corePriCrc[0] != corePriCrc[1])
        {
            Printf(Tee::PriHigh, "Primary CRC for Core didn't match after a pstate switch\n");
            return RC::GOLDEN_VALUE_MISCOMPARE;
        }
    }

    return OK;
}

// Gets all the available Pstate infos
RC PstateWithModeset::SetupPstatesInfo()
{
    // Get P-States info
    RC rc;
    LwU32 nMaxPState = 0;
    CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                LW2080_CTRL_CMD_PERF_GET_PSTATES_INFO,
                &m_getAllPStatesInfoParams,
                sizeof(m_getAllPStatesInfoParams)));

    nMaxPState = GetLogBase2(LW2080_CTRL_PERF_PSTATES_MAX);
    for (LwU32 i = 0; i <= nMaxPState; ++i)
    {
        if (m_getAllPStatesInfoParams.pstates & (1 << i))
        {
            LwU32 nPStateMask = 1 << i;
            m_vPStateMasks.push_back(nPStateMask);
        }
    }
    return OK;
}

RC PstateWithModeset::Setup()
{
    RC rc;
    m_hBasicSubDev = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    m_hBasicClient = pLwRm->GetClientHandle();
    CHECK_RC(SetupPstatesInfo());

    if (m_bIsDisplayAvailable)
    {
       CHECK_RC(GetDisplayResolutions());
    }

    if (m_vPStateMasks.size() == 0)
    {
        Printf(Tee::PriHigh, "Error: %s - No pstates available\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

RC PstateWithModeset::SwitchPstates(LwU32 minPstate, bool bUseMinPstate)
{
    RC    rc  = OK;
    LwU32 lwrrPstate = 0;
    LwU32 Fallback = LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR;

    m_lwrrentPstateSwitches = 0;
    for (LwS32 i = ((LwS32)m_vPStateMasks.size()) - 1; i >= 0; i--)
    {
        if((bUseMinPstate==TRUE) && (minPstate < m_vPStateMasks[i]))
            continue;
        for (LwS32 j = ((LwS32)m_vPStateMasks.size()) - 1; j >= 0; j--)
        {
            if((bUseMinPstate == TRUE) && ((minPstate < m_vPStateMasks[j]) || (j==i)))
                continue;

            if (m_lwrrentPstateSwitches >= m_maxPstateSwitches)
               break;

            Printf(Tee::PriHigh, "***************Transition from P%u -> P%u**************\n",
                                             GetLogBase2(m_vPStateMasks[i]), GetLogBase2(m_vPStateMasks[j]));
            CHECK_RC(GetLwrrentPstate(lwrrPstate, Fallback));
            if(lwrrPstate != m_vPStateMasks[i])
                CHECK_RC(ProgramPstateAndCompareCRC(m_vPStateMasks[i], Fallback));
            CHECK_RC(ProgramPstateAndCompareCRC(m_vPStateMasks[j], Fallback));
            Printf(Tee::PriHigh, "********************Transition end**********************\n\n");

        }
     }
     CHECK_RC(GetLwrrentPstate(lwrrPstate, Fallback));
     if(lwrrPstate != m_vPStateMasks[0])
     {
         // Assuming m_vPStateMasks[0] generally holds the higher pstate, set it to support next modeset
         Printf(Tee::PriHigh,"Setting P%u to support next Modeset\n", GetLogBase2(m_vPStateMasks[0]));
         CHECK_RC(ProgramPstateAndCompareCRC(m_vPStateMasks[0], Fallback));
     }
     Printf(Tee::PriHigh, "PstateWithModeset: Total amount of pstate transitions in this mode - %u\n", m_lwrrentPstateSwitches);

     return rc;
}

//Loops through all resolutions and sets possible pstates.
RC PstateWithModeset::RunBasicPstateWithModesetTest()
{
    RC    rc            = OK;
    LwU32 resCount      = 0;
    LwU32 loopCnt       = 0;
    LwU32 totalModesets = 0;
    AcrVerify           acrVerify;

    CHECK_RC(acrVerify.BindTestDevice(GetBoundTestDevice()));

    if (m_bIsDisplayAvailable != TRUE)
    {
        Printf(Tee::PriHigh, "Starting all possible P-State transitions with no display modesets:\n");
        return SwitchPstates(0, FALSE);
    }

    Printf(Tee::PriHigh, "Starting all possible P-State transitions with display modesets:\n");

    for(loopCnt = 0; loopCnt < m_loopCount; loopCnt++)
    {
        totalModesets = 0;
        for(resCount = 0; resCount < m_totalResCount; resCount++)
        {
            EvoLutSettings *pLutSettings;
            LwU32          minPstate;

            if((!m_allModes)&&((m_widths[resCount] > m_maxWidth) ||(m_heights[resCount] > m_maxHeight) ||
                              (m_widths[resCount] < m_minWidth) || (m_heights[resCount] < m_minHeight)||
                              (m_refreshRates[resCount] > m_maxRefreshRate) || (m_refreshRates[resCount] < m_minRefreshRate)))

            {
                Printf(Tee::PriHigh,"Skipping mode %ux%u @%u\n",m_widths[resCount], m_heights[resCount],
                                                                          m_refreshRates[resCount]);
                continue;
            }

            totalModesets += 1;

            Printf(Tee::PriHigh,"Setting mode %ux%u @%u\n",m_widths[resCount], m_heights[resCount],
                                                                              m_refreshRates[resCount]);
            CHECK_RC(m_pDisplay->SetMode(selectedDisplay, m_widths[resCount],
                                 m_heights[resCount], m_depth, m_refreshRates[resCount]));
            CHECK_RC(AllocSetLUTAndGamma(selectedDisplay, pLutSettings));
            CHECK_RC(m_pDisplay->GetIMPMinPState(selectedDisplay, (UINT32*)&minPstate));
            Printf(Tee::PriHigh,"Min Pstate: P%u\n",GetLogBase2(minPstate));
            CHECK_RC(SwitchPstates(minPstate, TRUE));
            CHECK_RC(FreeLUT(selectedDisplay, pLutSettings));
        }

        // Verify security level of PMU
        if( acrVerify.IsTestSupported() ==  RUN_RMTEST_TRUE )
        {
            acrVerify.Setup();
            if(acrVerify.VerifyFalconStatus(LSF_FALCON_ID_PMU, LW_FALSE) != OK)
            {
                Printf(Tee::PriHigh, "Error: %s PMU Failed to Boot in Expected Security Mode\n",
                                __FUNCTION__);
                return RC::SOFTWARE_ERROR;
            }
        }

        if (!totalModesets)
        {
            Printf(Tee::PriHigh, "Error: %s Total modesets is 0\n", __FUNCTION__);
            rc = RC::SOFTWARE_ERROR;
            break;
        }

    }

    return rc;
}

RC PstateWithModeset::Run()
{
    RC rc = OK;
    CHECK_RC(RunBasicPstateWithModesetTest());
    return rc;
}

RC PstateWithModeset::Cleanup()
{
   if (m_bIsDisplayAvailable)
   {
       delete m_widths;
       delete m_heights;
       delete m_refreshRates;
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
JS_CLASS_INHERIT(PstateWithModeset, RmTest, "PstateWithModeset test\n");
CLASS_PROP_READWRITE(PstateWithModeset, maxWidth, int,
                     "Max width used in setmode, default= 1280");
CLASS_PROP_READWRITE(PstateWithModeset, maxHeight, int,
                     "Max height used in setmode, default= 800");
CLASS_PROP_READWRITE(PstateWithModeset, maxRefreshRate, int,
                     "Max refreshRate used in setmode, default= 60");
CLASS_PROP_READWRITE(PstateWithModeset, depth, int,
                     "depth used in setmode, default= 32");
CLASS_PROP_READWRITE(PstateWithModeset, allModes, int,
                     "Use all modes for setmode, default= 0");
CLASS_PROP_READWRITE(PstateWithModeset, loopCount, int,
                     "Number of loops for modeset and pstate switch, default= 1");
CLASS_PROP_READWRITE(PstateWithModeset, edidFile, string,
                     "Force using custom EDID instead of loading from RM");
CLASS_PROP_READWRITE(PstateWithModeset, minWidth, int,
                     "Min width used in setmode, default= 0");
CLASS_PROP_READWRITE(PstateWithModeset, minHeight, int,
                     "Min height used in setmode, default= 0");
CLASS_PROP_READWRITE(PstateWithModeset, minRefreshRate, int,
                     "Min refreshRate used in setmode, default= 0");
CLASS_PROP_READWRITE(PstateWithModeset, maxPstateSwitches, int,
                     "Maximum number of pstate transitions for a single mode, default= 2");

