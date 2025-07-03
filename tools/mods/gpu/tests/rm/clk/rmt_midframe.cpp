/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2006-2009, 2011-2014 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//!
//!
//! \file rmt_midframe.txt
//! \brief a test to verify if mclk switch happens during midframe
//! Single Head (numHeads = 1) : It verify mclk switch is happening properly or for all the possible pstate transitions
//!                              and dumps various stats. This doesn't compare the CRC before and after p-state transition
//!
//! Dual Head (numHeads = 2)   : It verify mclk switch is happening properly or for all the possible pstate transitions
//!                              and compares the crc before and after p-state transiton.
//!

#include "gpu/tests/rmtest.h"
#include "core/include/display.h"
#include "core/include/utility.h"
#include "gpu/tests/rm/utility/crccomparison.h"
#include "core/include/modsedid.h"
#include <string>
#include "core/include/platform.h"
#include "gpu/include/gpusbdev.h"
#include "rmt_fermipstateswitch.h"
#include "gpu/tests/rm/dispstatecheck/dispstate.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/display/evo_disp.h"
#include "core/include/memcheck.h"
/*
 * This test loops through all possible resolutions and pstates for each mode.
 * After every pstate, profiling data including DMIPOS, PMU timeout are collected using a control call.
 * Failing criterion for this test:
 *    1. DMI pos is VBLANK and not active
 *    2. PMU times out
 *    3. Display underflows
 */

typedef struct
{
    LwU32 resIndex;
    LwU32 pState;
    LW2080_CTRL_CLK_GET_PROFILING_DATA_PARAMS profilingParams;
}MidframeProfileData;

class Midframe:public RmTest
{
public:
    Midframe();
    virtual ~Midframe();
    virtual string  IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    RC    SetupPstatesInfo();
    RC    ProgramPstate(LwU32, LwU32);
    RC    ProgramPstateAndCompareCRC(LwU32, LwU32);
    RC    RunBasicMidframeTest();
    RC    GetDisplayResolutions();
    RC    GetCRCValuesAndDump(DisplayID, EvoCRCSettings::CHANNEL,
                                  LwU32&, LwU32&, string, bool);
    RC    CompareDMIPosWithPstateSwitch(LwU32, LwU32, LwU32);
    RC    AllocSetLUTAndGamma(DisplayID Display,
                               EvoLutSettings *&pLutSettings);
    RC    FreeLUT(DisplayID Display,
              EvoLutSettings *pLutSettings);
    RC    GetLwrrentPstate(LwU32&, LwU32);
    RC    AnalyzeStats();
    LwU32 GetLogBase2(LwU32);

    SETGET_PROP(maxWidth, LwU32);
    SETGET_PROP(maxHeight, LwU32);
    SETGET_PROP(maxRefreshRate, LwU32);
    SETGET_PROP(depth, LwU32);
    SETGET_PROP(allModes, LwU32);
    SETGET_PROP(loopCount, LwU32);
    SETGET_PROP(numHeads, LwU32);
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
    vector<DisplayID> selectedDisplay;
    string        m_protocol;
    LwU32         m_numHeads;

    //! P-States parameter structures for RM ctrl cmds
    LW2080_CTRL_PERF_GET_PSTATES_INFO_PARAMS m_getAllPStatesInfoParams;
    LW2080_CTRL_PERF_GET_LWRRENT_PSTATE_PARAMS m_getLwrrentPStateParams;
    LW2080_CTRL_PERF_SET_FORCE_PSTATE_PARAMS m_setForcePStateParams;
    vector<LwU32> m_vPStateMasks;

    struct ModesetInfo
    {
        //Modeset details
        LwU32 *widths;
        LwU32 *heights;
        LwU32 *refreshRates;
        LwU32 totalResCount;

        ModesetInfo() : widths(NULL),
            heights(NULL),
            refreshRates(NULL),
            totalResCount(0)
        {
        }
    };

    LwU32   m_maxWidth;
    LwU32   m_maxHeight;
    LwU32   m_maxRefreshRate;

    LwU32   m_minWidth;
    LwU32   m_minHeight;
    LwU32   m_minRefreshRate;
    LwU32   m_depth;
    LwU32   m_maxPstateSwitches;
    LwU32   m_lwrrentPstateSwitches;

    LwU32   m_forceLwstomEdid;
    string  m_edidFile;

    vector<ModesetInfo> m_Modesetinfos;

    LwU32   m_allModes;
    LwU32   m_loopCount;

    bool    m_runningOnSim;

    //Stats
    vector<MidframeProfileData*> statProfiles;
    map<LwU32, LwU32>            dmiPosCounters;
    map<LwU32, LwU32>            *dmiPosWrtRes;
};

/*
 * Set default values for modeset variables.
 */

Midframe::Midframe()
{
    SetName("Midframe");
    m_pSubdevice = GetBoundGpuSubdevice();
    // Setting invalid max values
    m_maxWidth      = 0xFFFF;
    m_maxHeight     = 0xFFFF;
    m_maxRefreshRate= 0xFF;
    m_depth         = 32;
    m_allModes      = 0; //Don't loop through all resolutions. Use max width, height and refreshrate to restrict.
    m_loopCount     = 1;
    m_numHeads      = 1;
    dmiPosWrtRes    = NULL;

    m_forceLwstomEdid = 0;
    m_minWidth      = 0;
    m_minHeight     = 0;
    m_minRefreshRate= 0;
    m_maxPstateSwitches = 2;

}

Midframe::~Midframe()
{

}

//Gets all resolutions for selectedDisplay
RC Midframe::GetDisplayResolutions()
{
    RC rc;
    Edid *pEdid = m_pDisplay->GetEdidMember();
    LwU32 i;

    for(i = 0; i < m_numHeads; ++i)
    {
        if (!m_runningOnSim)
        {
           CHECK_RC(pEdid->GetNumListedResolutions(selectedDisplay[i], (UINT32*)&m_Modesetinfos[i].totalResCount));

            if(!m_Modesetinfos[i].totalResCount)
            {
                Printf(Tee::PriHigh, "No resolutions found\n");
                return RC::SOFTWARE_ERROR;
            }
        }
        else
        {
            m_Modesetinfos[i].totalResCount = 1;
        }

        m_Modesetinfos[i].widths       = new LwU32[m_Modesetinfos[i].totalResCount];
        m_Modesetinfos[i].heights      = new LwU32[m_Modesetinfos[i].totalResCount];
        m_Modesetinfos[i].refreshRates = new LwU32[m_Modesetinfos[i].totalResCount];

        if (!m_runningOnSim)
        {
            CHECK_RC(pEdid->GetListedResolutions(selectedDisplay[i], (UINT32*)m_Modesetinfos[i].widths,
                (UINT32*)m_Modesetinfos[i].heights,
                (UINT32*)m_Modesetinfos[i].refreshRates));

        }
        else
        {
           m_Modesetinfos[i].widths[0]       = 800;
           m_Modesetinfos[i].heights[0]      = 600;
           m_Modesetinfos[i].refreshRates[0] = 60;
        }
    }

    return OK;
}

/*
 * This test is supported for single head mid frame switching for GF119 and dual head mid frame switching for Kepler.
 */
string Midframe::IsTestSupported()
{
    DisplayIDs Detected;
    LwU32      i;
    m_pDisplay = GetDisplay();

    if(m_pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS) != OK)
        return "Initializing display HW failed";

    // get all the connected displays
    if(m_pDisplay->GetDetected(&Detected) != OK)
        return "Detecting displays failed";

    Printf(Tee::PriHigh, "Number of detected displays = %u\n", (LwU32)Detected.size());

    if(Detected.size() == 0)
    {
        Printf(Tee::PriHigh, "Test supported with more than one display \n");
        return "Test supported only with more than one display";
    }

    if (strcmp(m_edidFile.c_str(), ""))
    {
        Printf(Tee::PriHigh, "Forcing custom EDID file %s\n", m_edidFile.c_str());
        m_forceLwstomEdid = 1;
    }

    m_Modesetinfos.resize(m_numHeads);

    if (m_numHeads)
    {
        if ((!m_pSubdevice->HasFeature(Device::GPUSUB_SUPPORTS_DUAL_HEAD_MIDFRAME_SWITCH)) &&
                  (m_numHeads == 2))
        {
            Printf(Tee::PriLow,"Dual Head Mid frame switch  is supported only on GK104/7 only \n");
            return "GPUSUB_SUPPORTS_DUAL_HEAD_MIDFRAME_SWITCH device feature is not supported on current platform";
        }
        else if (m_numHeads > 2)
        {
            Printf(Tee::PriLow,"Multi Head Mid frame switch is not yet supported \n");
            return "Multi Head (> 2)  Mid frame switch is not yet supported on any platform";
        }

        for(i = 0; i < Detected.size(); ++i)
        {
            //
            // if the edid is not valid for disp id then SetLwstomEdid.
            // This is needed on emulation / simulation / silicon (when kvm used)
            //
            if (m_forceLwstomEdid)
            {
                if (DTIUtils::EDIDUtils::SetLwstomEdid(m_pDisplay, Detected[i], true, m_edidFile) != OK)
                {
                    Printf(Tee::PriLow,"SetLwstomEdid failed for displayID = 0x%X \n",
                        (UINT32)Detected[i]);
                    return "SetLwstomEdid failed";
                }
            }
            else if (! (DTIUtils::EDIDUtils::IsValidEdid(m_pDisplay, Detected[i]) &&
                   DTIUtils::EDIDUtils::EdidHasResolutions(m_pDisplay, Detected[i]))
               )
            {
                if (DTIUtils::EDIDUtils::SetLwstomEdid(m_pDisplay, Detected[i], true) != OK)
                {
                    Printf(Tee::PriLow,"SetLwstomEdid failed for displayID = 0x%X \n",
                        (UINT32)Detected[i]);
                    return "SetLwstomEdid failed";
                }
            }
            selectedDisplay.push_back(Detected[i]);
        }
    }
    else
    {
        Printf(Tee::PriHigh, " Invalid numHeadsUsable \n");
        return "Test supported only for numHeadsUsable > 0";
    }

    return RUN_RMTEST_TRUE;
}

LwU32 Midframe::GetLogBase2(LwU32 nNum)
{
    return (LwU32)ceil(log((double)nNum)/log(2.0));
}

/*
 * Prints the stats and checks if DMIPOS is Vblank or active.
 */

RC Midframe::AnalyzeStats()
{
    LwU32 i             = 0;
    LwU32 totProfiles   = 0;
    bool  isActive      = false;
    LwU32 nonZeroDmiCnt = 0;

    if (m_numHeads ==1)
    {
        Printf(Tee::PriHigh, "\n\n");
        Printf(Tee::PriHigh,"Sno   Resolution   Perf  DmiPos      RGDpca      FN     LC     FBStart     FBStop Diff(ns)  Is DMI in ACTIVE / VBLANK\n");
        Printf(Tee::PriHigh,"----------------------------------------------------------------------------------------------\n");
        while( i < statProfiles.size() )
        {
            LwU32 tmp = 0;
            MidframeProfileData *profile = statProfiles[i];
            LW2080_CTRL_CLK_GET_PROFILING_DATA_PARAMS pParams = profile->profilingParams;
            totProfiles += 1;
            Printf(Tee::PriHigh,"%04u  %04ux%04u@%02u  P%02u  0x%08x  0x%08x  %05u  %05u  0x%08x  0x%08x  %-10d  ",
                        totProfiles, m_Modesetinfos[0].widths[profile->resIndex], m_Modesetinfos[0].heights[profile->resIndex],
                        (m_Modesetinfos[0].refreshRates[profile->resIndex]<m_maxRefreshRate)?m_Modesetinfos[0].refreshRates[profile->resIndex]:m_maxRefreshRate,
                        GetLogBase2(profile->pState),
                        pParams.dmiPos, pParams.RGDpca,
                        (pParams.RGDpca)>>16, ((pParams.RGDpca)<<16)>>16,
                        (UINT32)pParams.FBStartTime, (UINT32)pParams.FBStopTime,
                        (UINT32)(pParams.FBStopTime - pParams.FBStartTime));

            if(BIT(31) & pParams.dmiPos)
            {
                Printf(Tee::PriHigh, "VBLANK ****\n");
                //Printf(Tee::PriHigh, "Error: DMI Pos is VBLANK\n");
                //return RC::SOFTWARE_ERROR;
            }
            else
            {
                isActive = true;
                Printf(Tee::PriHigh, "ACTIVE\n");
            }
            if(dmiPosCounters.count(pParams.dmiPos))
                tmp = dmiPosCounters[pParams.dmiPos];
            dmiPosCounters[pParams.dmiPos] = tmp + 1;
            tmp = 0;
            if(dmiPosWrtRes[profile->resIndex].count(pParams.dmiPos))
                tmp = dmiPosWrtRes[profile->resIndex][pParams.dmiPos];
            dmiPosWrtRes[profile->resIndex][pParams.dmiPos] = tmp + 1;
            delete profile;
            ++i;
        }

        if(!isActive)
        {
            Printf(Tee::PriHigh, "Error: DMI pos is always VBLANK\n");
            return RC::SOFTWARE_ERROR;
        }

        Printf(Tee::PriHigh, "\n\n");
        Printf(Tee::PriHigh, "      DMI Position Summary \n\n");
        Printf(Tee::PriHigh, "DMI Position   Count\n");
        Printf(Tee::PriHigh, "-------------------------\n");
        for(map<LwU32, LwU32>::iterator it = dmiPosCounters.begin(); it != dmiPosCounters.end(); ++it)
        {
            Printf(Tee::PriHigh,"0x%08x     %u\n", it->first, it->second);
        }
        Printf(Tee::PriHigh, "\n");
        Printf(Tee::PriHigh, "\n");

        if(dmiPosCounters.size() == 1)
        {
            Printf(Tee::PriHigh, "Error: DMI pos is always constant.\n");
            return RC::SOFTWARE_ERROR;
        }
        Printf(Tee::PriHigh, "DMI position summary per resolution\n\n");
        Printf(Tee::PriHigh, "Resolution    DMI Pos     Count\n");
        Printf(Tee::PriHigh, "---------------------------------\n");
        for(i = 0; i < m_Modesetinfos[0].totalResCount; ++i)
        {
            bool resPrinted = false, nzDmi = false;
            Printf(Tee::PriHigh,"%04ux%04u@%02u  ", m_Modesetinfos[0].widths[i], m_Modesetinfos[0].heights[i], m_Modesetinfos[0].refreshRates[i]);
            for(map<LwU32, LwU32>::iterator pt = dmiPosWrtRes[i].begin(); pt != dmiPosWrtRes[i].end(); ++pt)
            {
                if(resPrinted)
                    Printf(Tee::PriHigh,"              ");
                resPrinted = true;

                //Make sure we get non-zero Dmi pos for more than one resolution. From Dhawal
                if((!nzDmi) && (pt->first))
                {
                    nonZeroDmiCnt += 1;
                    nzDmi = true;
                }
                Printf(Tee::PriHigh, "0x%08x  %u\n", pt->first, pt->second);
            }
            if(!resPrinted)
                Printf(Tee::PriHigh, "\n");
        }
        Printf(Tee::PriHigh, "---------------------------------------\n");
        Printf(Tee::PriHigh, "Nonzero DMI positions count: %u\n", nonZeroDmiCnt);

        if(nonZeroDmiCnt < 1)
        {
            Printf(Tee::PriHigh, "Error: No nonzero DMI pos for more than one resolution.\n");
            return RC::SOFTWARE_ERROR;
        }
    }
    else
    {
        // Free collected profiles
        while( i < statProfiles.size() )
        {
            delete statProfiles[i];
            ++i;
        }
    }
    return OK;
}

/* Gets the current Pstate
 */
RC Midframe::GetLwrrentPstate(LwU32 &lwrrPstate, LwU32 fallback)
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
RC Midframe::ProgramPstate(LwU32 targetPstate, LwU32 fallback)
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
        Printf(Tee::PriHigh, "%d:Forced transition to P-State P%u (Mask 0x%08x)failed\n", __LINE__,
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

RC Midframe::AllocSetLUTAndGamma(DisplayID Display,
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

RC Midframe::FreeLUT(DisplayID Display,
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
RC Midframe::GetCRCValuesAndDump
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
RC Midframe::ProgramPstateAndCompareCRC(LwU32 targetPstate, LwU32 fallback)
{
    RC rc;
    LwU32 coreORCrc[4], corePriCrc[4];
    LwU32 i;

    // Removing CRC check for single head
    if ( m_numHeads > 1)
    {
        for(i = 0; i < m_numHeads; ++i)
        {
            CHECK_RC(GetCRCValuesAndDump(selectedDisplay[i], EvoCRCSettings::CORE, coreORCrc[i],
                                                                 corePriCrc[i], "coreCrc1.xml", true));
        }
    }

    CHECK_RC(ProgramPstate(targetPstate, fallback));

    if (m_numHeads > 1)
    {
        for(i = 0; i < m_numHeads; ++i)
        {
            CHECK_RC(GetCRCValuesAndDump(selectedDisplay[i], EvoCRCSettings::CORE, coreORCrc[m_numHeads+i],
                                                                corePriCrc[m_numHeads+i], "coreCrc2.xml", true));
        }

        for(i = 0; i < m_numHeads; ++i)
        {
            if(coreORCrc[i] != coreORCrc[m_numHeads+i])
            {
                Printf(Tee::PriHigh, "OR CRC for Core didn't match after a pstate switch\n");
                return RC::GOLDEN_VALUE_MISCOMPARE;
            }

            if(corePriCrc[i] != corePriCrc[m_numHeads+i])
            {
                Printf(Tee::PriHigh, "Primary CRC for Core didn't match after a pstate switch\n");
                return RC::GOLDEN_VALUE_MISCOMPARE;
            }
        }
    }

    return OK;
}

// Gets all the available Pstate infos
RC Midframe::SetupPstatesInfo()
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

RC Midframe::Setup()
{
    RC rc;
    m_hBasicSubDev = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    m_hBasicClient = pLwRm->GetClientHandle();

    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        m_runningOnSim = true;
    }
    else
    {
        m_runningOnSim = false;
    }

    CHECK_RC(SetupPstatesInfo());
    CHECK_RC(GetDisplayResolutions());
    dmiPosWrtRes   = new map<LwU32, LwU32>[ m_Modesetinfos[0].totalResCount];

    return OK;
}

// Switches Pstate and collects profiling information using the control call.
// Checks for PMU timeout after every pstate switch
RC Midframe::CompareDMIPosWithPstateSwitch(LwU32 targetPstate, LwU32 fallback, LwU32 resIndex)
{
    RC rc;
    MidframeProfileData *profile = new MidframeProfileData;
    (profile->profilingParams).dmiPos = 0;
    (profile->profilingParams).FBStartTime= 0;
    (profile->profilingParams).FBStopTime= 0;
    CHECK_RC(ProgramPstateAndCompareCRC(targetPstate, fallback));
    CHECK_RC(pLwRm->Control(m_hBasicSubDev,
             LW2080_CTRL_CMD_CLK_GET_PROFILING_DATA,
             &(profile->profilingParams),
             sizeof(profile->profilingParams)));
    profile->pState = targetPstate;
    profile->resIndex = resIndex;
    statProfiles.push_back(profile);
    m_lwrrentPstateSwitches += 1;
    if((profile->profilingParams).pmuTimeout)
    {
        Printf(Tee::PriHigh, "Error: PMU Timed out with value %u\n",
               (unsigned int) profile->profilingParams.pmuTimeout);
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

//Loops through all resolutions and sets possible pstates.
RC Midframe::RunBasicMidframeTest()
{
    RC rc;
    LwU32 Fallback = LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR;
    LwU32 resCount = 0;
    LwU32 loopCnt  = 0;
    LwU32 resCount2, resIndex = 0;

    if (m_vPStateMasks.size() != 0)
    {
        Printf(Tee::PriHigh, "Starting all possible P-State transitions:\n");
    }
    else
    {
        Printf(Tee::PriHigh,
               "No P-States defined, so no P-State transitions possible\n");
        return OK;
    }
    for(loopCnt = 0; loopCnt < m_loopCount; loopCnt++)
    {
        if (m_numHeads == 1)
        {
            for(resCount = 0; resCount < m_Modesetinfos[0].totalResCount; resCount++)
            {
                EvoLutSettings *pLutSettings;
                LwU32          lwrrPstate;
                LwU32          minPstate;

                if((!m_allModes)&&((m_Modesetinfos[0].widths[resCount] > m_maxWidth) ||(m_Modesetinfos[0].heights[resCount] > m_maxHeight) ||
                                   (m_Modesetinfos[0].widths[resCount] < m_minWidth) || (m_Modesetinfos[0].heights[resCount] < m_minHeight)||
                                   (m_Modesetinfos[0].refreshRates[resCount] > m_maxRefreshRate) || (m_Modesetinfos[0].refreshRates[resCount] < m_minRefreshRate)))
                    continue;

                Printf(Tee::PriHigh,"Setting mode %ux%u @%u\n",m_Modesetinfos[0].widths[resCount], m_Modesetinfos[0].heights[resCount],
                                                                                  m_Modesetinfos[0].refreshRates[resCount]);
                CHECK_RC(m_pDisplay->SetMode(selectedDisplay[0], m_Modesetinfos[0].widths[resCount],
                                     m_Modesetinfos[0].heights[resCount], m_depth, m_Modesetinfos[0].refreshRates[resCount]));
                CHECK_RC(AllocSetLUTAndGamma(selectedDisplay[0], pLutSettings));

                CHECK_RC(m_pDisplay->GetIMPMinPState(selectedDisplay[0], (UINT32*)&minPstate));
                Printf(Tee::PriHigh,"Min Pstate: P%u\n",GetLogBase2(minPstate));
                m_lwrrentPstateSwitches = 0;
                for (LwS32 i = ((LwS32)m_vPStateMasks.size()) - 1; i >= 0; i--)
                {
                    if(minPstate < m_vPStateMasks[i])
                        continue;
                    for (LwS32 j = ((LwS32)m_vPStateMasks.size()) - 1; j >= 0; j--)
                    {
                        if((minPstate < m_vPStateMasks[j]) || (j==i))
                            continue;

                        if (m_lwrrentPstateSwitches >= m_maxPstateSwitches)
                            break;

                        Printf(Tee::PriHigh, "***************Transition from P%u -> P%u**************\n",
                                             GetLogBase2(m_vPStateMasks[i]), GetLogBase2(m_vPStateMasks[j]));
                        CHECK_RC(GetLwrrentPstate(lwrrPstate, Fallback));
                        if(lwrrPstate != m_vPStateMasks[i])
                            CHECK_RC(CompareDMIPosWithPstateSwitch(m_vPStateMasks[i], Fallback, resCount));
                        CHECK_RC(CompareDMIPosWithPstateSwitch(m_vPStateMasks[j], Fallback, resCount));
                        Printf(Tee::PriHigh, "********************Transition end**********************\n\n");

                    }
                }
                CHECK_RC(GetLwrrentPstate(lwrrPstate, Fallback));
                if(lwrrPstate != m_vPStateMasks[0])
                {
                    Printf(Tee::PriHigh,"Setting P%u to support next Modeset\n", GetLogBase2(m_vPStateMasks[0]));
                    CHECK_RC(CompareDMIPosWithPstateSwitch(m_vPStateMasks[0], Fallback, resCount));
                }
                Printf(Tee::PriHigh, "Midframe: Total amount of pstate transitions in this mode - %u\n", m_lwrrentPstateSwitches);
                CHECK_RC(FreeLUT(selectedDisplay[0], pLutSettings));
            }
        }
        else if (m_numHeads == 2)
        {
            for(resCount = 0; resCount < m_Modesetinfos[0].totalResCount; resCount++)
            {
                EvoLutSettings *pLutSettings1;
                EvoLutSettings *pLutSettings2;
                LwU32          lwrrPstate;
                LwU32          minPstate;
                if((!m_allModes)&&((m_Modesetinfos[0].widths[resCount] > m_maxWidth) ||(m_Modesetinfos[0].heights[resCount] > m_maxHeight) ||
                                   (m_Modesetinfos[0].widths[resCount] < m_minWidth) || (m_Modesetinfos[0].heights[resCount] < m_minHeight)||
                                   (m_Modesetinfos[0].refreshRates[resCount] > m_maxRefreshRate) || (m_Modesetinfos[0].refreshRates[resCount] < m_minRefreshRate)))
                    continue;
                Printf(Tee::PriHigh,"Setting mode %ux%u @%u\n",m_Modesetinfos[0].widths[resCount], m_Modesetinfos[0].heights[resCount],
                                                                                  m_Modesetinfos[0].refreshRates[resCount]);
                CHECK_RC(m_pDisplay->SetMode(selectedDisplay[0], m_Modesetinfos[0].widths[resCount],
                                     m_Modesetinfos[0].heights[resCount], m_depth, m_Modesetinfos[0].refreshRates[resCount]));
                CHECK_RC(AllocSetLUTAndGamma(selectedDisplay[0], pLutSettings1));

                for(resCount2 = 0; resCount2 < m_Modesetinfos[1].totalResCount; resCount2++)
                {
                    if((!m_allModes)&&((m_Modesetinfos[1].widths[resCount2] > m_maxWidth) ||(m_Modesetinfos[1].heights[resCount2] > m_maxHeight) ||
                                       (m_Modesetinfos[1].widths[resCount2] < m_minWidth) || (m_Modesetinfos[1].heights[resCount2] < m_minHeight)||
                                       (m_Modesetinfos[1].refreshRates[resCount2] > m_maxRefreshRate) || (m_Modesetinfos[1].refreshRates[resCount2] < m_minRefreshRate)))
                        continue;
                    Printf(Tee::PriHigh,"Setting mode %ux%u @%u\n",m_Modesetinfos[1].widths[resCount2], m_Modesetinfos[1].heights[resCount2],
                                                                   m_Modesetinfos[1].refreshRates[resCount2]);

                    CHECK_RC(m_pDisplay->SetMode(selectedDisplay[1], m_Modesetinfos[1].widths[resCount2],
                                                 m_Modesetinfos[1].heights[resCount2], m_depth, m_Modesetinfos[1].refreshRates[resCount2]));
                    CHECK_RC(AllocSetLUTAndGamma(selectedDisplay[1], pLutSettings2));

                    CHECK_RC(m_pDisplay->GetIMPMinPState(selectedDisplay[0], (UINT32*)&minPstate));
                    Printf(Tee::PriHigh,"Min Pstate: P%u\n",GetLogBase2(minPstate));
                    m_lwrrentPstateSwitches = 0;
                    for (LwS32 i = ((LwS32)m_vPStateMasks.size()) - 1; i >= 0; i--)
                    {
                        if(minPstate < m_vPStateMasks[i])
                            continue;
                        for (LwS32 j = ((LwS32)m_vPStateMasks.size()) - 1; j >= 0; j--)
                        {
                            if((minPstate < m_vPStateMasks[j]) || (j==i))
                                continue;

                            if (m_lwrrentPstateSwitches >= m_maxPstateSwitches)
                                break;

                            Printf(Tee::PriHigh, "***************Transition from P%u -> P%u**************\n",
                                                 GetLogBase2(m_vPStateMasks[i]), GetLogBase2(m_vPStateMasks[j]));
                            CHECK_RC(GetLwrrentPstate(lwrrPstate, Fallback));
                            resIndex = m_Modesetinfos[0].totalResCount*resCount + resCount2;
                            if(lwrrPstate != m_vPStateMasks[i])
                                CHECK_RC(CompareDMIPosWithPstateSwitch(m_vPStateMasks[i], Fallback, resIndex));
                            CHECK_RC(CompareDMIPosWithPstateSwitch(m_vPStateMasks[j], Fallback, resIndex));
                            Printf(Tee::PriHigh, "********************Transition end**********************\n\n");
                        }
                    }
                    CHECK_RC(GetLwrrentPstate(lwrrPstate, Fallback));
                    if(lwrrPstate != m_vPStateMasks[0])
                    {
                        Printf(Tee::PriHigh,"Setting P%u to support next Modeset\n", GetLogBase2(m_vPStateMasks[0]));
                        CHECK_RC(CompareDMIPosWithPstateSwitch(m_vPStateMasks[0], Fallback, resIndex));
                    }
                    Printf(Tee::PriHigh, "Midframe: Total amount of pstate transitions in this mode - %u\n", m_lwrrentPstateSwitches);
                    CHECK_RC(FreeLUT(selectedDisplay[1], pLutSettings2));
                }
                CHECK_RC(FreeLUT(selectedDisplay[0], pLutSettings1));
            }
        }
   }
    return OK;
}

RC Midframe::Run()
{
    RC rc;
    CHECK_RC(RunBasicMidframeTest());
    CHECK_RC(AnalyzeStats());
    return OK;
}

RC Midframe::Cleanup()
{
    LwU32 i = 0;

    for (i=0; i < m_numHeads; i++)
    {
        delete[] m_Modesetinfos[i].widths;
        delete[] m_Modesetinfos[i].heights;
        delete[] m_Modesetinfos[i].refreshRates;
    }

    if(dmiPosWrtRes)
    {
        delete[] dmiPosWrtRes;
        dmiPosWrtRes = NULL;
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
JS_CLASS_INHERIT(Midframe, RmTest, "Midframe clk switching test\n");

CLASS_PROP_READWRITE(Midframe, maxWidth, int,
                     "Max width used in setmode, default= 0");
CLASS_PROP_READWRITE(Midframe, maxHeight, int,
                     "Max height used in setmode, default= 0");
CLASS_PROP_READWRITE(Midframe, maxRefreshRate, int,
                     "Max refreshRate used in setmode, default= 0");
CLASS_PROP_READWRITE(Midframe, depth, int,
                     "depth used in setmode, default= 0");
CLASS_PROP_READWRITE(Midframe, allModes, int,
                     "Use all modes for setmode, default= 0");
CLASS_PROP_READWRITE(Midframe, loopCount, int,
                     "Loop through modesets, default= 1");
CLASS_PROP_READWRITE(Midframe, numHeads, int,
                     "Verify mid frame switching for numHeadsToUse passed, default= 1");
CLASS_PROP_READWRITE(Midframe, edidFile, string,
                     "Force using custom EDID instead of loading from RM");
CLASS_PROP_READWRITE(Midframe, minWidth, int,
                     "Min width used in setmode, default= 0");
CLASS_PROP_READWRITE(Midframe, minHeight, int,
                     "Min height used in setmode, default= 0");
CLASS_PROP_READWRITE(Midframe, minRefreshRate, int,
                     "Min refreshRate used in setmode, default= 0");
CLASS_PROP_READWRITE(Midframe, maxPstateSwitches, int,
                     "Maximum number of pstate transitions for a single mode, default= 2");
