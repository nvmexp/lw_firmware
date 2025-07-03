/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2010,2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_GlitchlessMclkSwitchRLTest.cpp
//! \Test Glitchless mclk switch on Raster Lock mode
//!
#include "core/include/modsedid.h"
#include "core/include/gpu.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/display/evo_disp.h"
#include "gpu/utility/surf2d.h"
#include "core/include/platform.h"
#include "disp/v02_00/dev_disp.h"
#include "core/include/memcheck.h"
#include "class/cl507dcrcnotif.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/tests/rm/clk/rmt_fermipstateswitch.h"
#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#define NUM_HEAD 2

class GlitchlessMclkSwitchRLTest : public RmTest
{
public:
    GlitchlessMclkSwitchRLTest();
    virtual ~GlitchlessMclkSwitchRLTest();

    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(height, UINT32);
    SETGET_PROP(width, UINT32);
    SETGET_PROP(protocol, string);
    SETGET_PROP(onlyConnectedDisplays,bool);

private:
    const string DISP_INFRA_IMAGES_DIR;
    const string coreChnImageFile;
    const string baseChnImageFile;
    const string ovlyChnImageFile;
    const string lwrsChnImageFile;
    Surface2D          m_CoreImage[NUM_HEAD];
    Surface2D          m_LwrsorImage[NUM_HEAD];
    Surface2D          m_OverlayImage[NUM_HEAD];

    UINT32       m_numIter;
    UINT32       m_width;
    UINT32       m_height;
    string       m_protocol;
    bool         m_onlyConnectedDisplays;

    FermiPStateSwitchTest   *m_pStateSwitch;
    Display                 *m_pDisplay;
    DisplayID               m_SelectedDisplay[NUM_HEAD];
    EvoLutSettings          *m_pLutSettings[NUM_HEAD];

    RC VerifyDisp();
    RC checkDispUnderflow();
    RC AllocSetLUTAndGamma(DisplayID Display, EvoLutSettings *&pLutSettings);
    RC FreeLUT(DisplayID Display, EvoLutSettings *pLutSettings);
    RC getGoldenCRC(DisplayID Display, UINT32 *pCrc);
    RC runMclkSwitchTest(UINT32 * pGoldencrcArray, DisplayID * pDisplayArray);
    void checkCRC(UINT32 head, DisplayID displayid, UINT32 goldencrc, UINT32& sum, UINT32& failed);
    void setRasterLockMode();

};

RC GlitchlessMclkSwitchRLTest::runMclkSwitchTest(UINT32 * pGoldencrcArray, DisplayID * pDisplayArray)
{
    RC rc;
    EvoCRCSettings     crcSettings[NUM_HEAD];
    UINT32 PStateSize;
    UINT32 i;
    UINT32 failed[NUM_HEAD];
    UINT32 sum[NUM_HEAD];
    float  rate[NUM_HEAD];
    UINT32 iter_failed;
    UINT32 iter_sum;

    //
    // To guarantee valid test results, first clear any pstate forces that may
    // have been left by other programs.
    //
    // Note: The RM Ctrl call for forced pstate clear does a pstate switch
    // internally to perfGetFastestLevel() value to restore clocks, in case
    // tools have changed any clocks directly.
    //
    GetBoundGpuSubdevice()->GetPerf()->ClearForcedPState();

    // p state
    CHECK_RC(m_pStateSwitch->PrintAllPStatesInfo());
    PStateSize = (UINT32) m_pStateSwitch->getVPStateMasks().size();
    //start test
    for(i=0; i<NUM_HEAD; i++)
    {
        crcSettings[i].ControlChannel = EvoCRCSettings::CORE;
        crcSettings[i].CrcPrimaryOutput = EvoCRCSettings::CRC_RG;
        CHECK_RC(m_pDisplay->SetupCrcBuffer(pDisplayArray[i], &crcSettings[i]));
        sum[i] = 0;
        failed[i] = 0;
    }
    for(UINT32 iter = 0; iter < m_numIter; iter ++){
        for(i=0; i<NUM_HEAD; i++)
            CHECK_RC(m_pDisplay->StartRunningCrc(pDisplayArray[i], &crcSettings[i]));
        // switch mclk
        for(UINT32 pstate = 0; pstate < PStateSize; pstate ++){
            CHECK_RC(m_pStateSwitch->ProgramAndVerifyPStates(m_pStateSwitch->getVPStateMasks()[pstate]));
            CHECK_RC(checkDispUnderflow());
            Tasker::Sleep(500);
        }
        for(i=0; i<NUM_HEAD; i++)
            CHECK_RC(m_pDisplay->StopRunningCrc(pDisplayArray[i], &crcSettings[i]));
        for(i=0; i<NUM_HEAD; i++)
        {
            checkCRC(i, pDisplayArray[i], pGoldencrcArray[i], iter_sum, iter_failed);
            sum[i] += iter_sum;
            failed[i] += iter_failed;
        }
    }
    CHECK_RC(m_pStateSwitch->ProgramPState(LW2080_CTRL_PERF_PSTATES_CLEAR_FORCED));
    for(i = 0; i<NUM_HEAD; i ++)
    {
        rate[i] = (float)failed[i] / (float) sum[i] *100;
        if(rate[i])
            Printf(Tee::PriHigh, "%s: Glitch Detected on head %d: %f%% CRC don't match.\n",
                __FUNCTION__, i ,rate[i]);
    }
    for(i = 0; i<NUM_HEAD; i ++)
    {
        if(rate[i])
            return RC::SOFTWARE_ERROR;
    }
    return rc;
}

void GlitchlessMclkSwitchRLTest::checkCRC(UINT32 head, DisplayID displayid, UINT32 goldencrc, UINT32& sum, UINT32& failed)
{
    UINT32 *pCrcBuf;
    UINT32 *pPrimaryCrcAddr;
    UINT32 lwrCrc;
    UINT32 crcOffset;
    failed = 0;
    pCrcBuf = (UINT32 *)m_pDisplay->GetCrcBuffer(displayid);
    sum = DRF_VAL(507D, _NOTIFIER_CRC_1, _STATUS_0_COUNT, MEM_RD32(pCrcBuf));
    crcOffset = LW507D_NOTIFIER_CRC_1_CRC_ENTRY0_2;
    for(UINT32 loopCount = 0; loopCount < sum; ++loopCount)
    {
        pPrimaryCrcAddr = pCrcBuf + crcOffset +
            LW507D_NOTIFIER_CRC_1_CRC_ENTRY0_4 - LW507D_NOTIFIER_CRC_1_CRC_ENTRY0_2;
        lwrCrc = MEM_RD32(pPrimaryCrcAddr);
        if (lwrCrc != goldencrc)
            failed ++;
        crcOffset += (LW507D_NOTIFIER_CRC_1_CRC_ENTRY1_6 -
                  LW507D_NOTIFIER_CRC_1_CRC_ENTRY0_2);
    }
}

RC GlitchlessMclkSwitchRLTest::getGoldenCRC(DisplayID displayid, UINT32 *pCrc)
{
    RC rc;
    UINT32 junk;
    EvoCRCSettings     crcSettings;
    crcSettings.ControlChannel = EvoCRCSettings::CORE;
    crcSettings.CrcPrimaryOutput = EvoCRCSettings::CRC_RG;
    CHECK_RC(m_pDisplay->SetupCrcBuffer(displayid, &crcSettings));
    CHECK_RC(m_pDisplay->GetEvoDisplay()->GetCrcValues(displayid, &crcSettings, &junk, pCrc, &junk));
    return rc;
}

//! \brief Constructor for GlitchlessMclkSwitchRLTest
//!
//! Set a name for the test
//!
//! \sa Setup
//------------------------------------------------------------------------------
GlitchlessMclkSwitchRLTest::GlitchlessMclkSwitchRLTest() :
        DISP_INFRA_IMAGES_DIR("dispinfradata/images/"),
        coreChnImageFile(DISP_INFRA_IMAGES_DIR + "coreimage640x480.PNG"),
        ovlyChnImageFile(DISP_INFRA_IMAGES_DIR + "ovlyimage640x480.PNG"),
        lwrsChnImageFile(DISP_INFRA_IMAGES_DIR + "lwrsorimage32x32.PNG")
{
    SetName("GlitchlessMclkSwitchRLTest");
    m_numIter = 0;
    m_width = 0;
    m_height = 0;
    m_protocol = "CRT,SINGLE_TMDS_A,SINGLE_TMDS_B,DUAL_TMDS,DP_A,DP_B";
    m_onlyConnectedDisplays = false;
    m_pStateSwitch = nullptr;
    m_pDisplay = nullptr;
    for(UINT32 i = 0; i < NUM_HEAD; i++)
    {
        m_SelectedDisplay[i] = 0;
        m_pLutSettings[i] = nullptr;
    }
}

//! \brief Destructor for GlitchlessMclkSwitchRLTest
//!
//! Does nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
GlitchlessMclkSwitchRLTest::~GlitchlessMclkSwitchRLTest()
{
}

//! \brief IsTestSupported()
//!
//! Check whether the hardware supports DMI_MEMACC functionality
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string GlitchlessMclkSwitchRLTest::IsTestSupported()
{
    Display *pDisplay = GetDisplay();
    if( pDisplay->GetDisplayClassFamily() == Display::EVO )
        return RUN_RMTEST_TRUE;
    return "EVO Display class is not supported on current platform";
}

//! \brief Setup
//!
//! Just JS settings
//
//! \return RC -> OK if everything is allocated,
//------------------------------------------------------------------------------
RC GlitchlessMclkSwitchRLTest::Setup()
{
    RC rc;
    CHECK_RC(InitFromJs());
    m_pDisplay = GetDisplay();
    CHECK_RC(m_pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));
    m_pStateSwitch = new FermiPStateSwitchTest();
    m_pStateSwitch->SetupFermiPStateSwitchTest();
    if (m_pStateSwitch->getVPStateMasks().size() < 2 )
    {
        Printf(Tee::PriHigh, "%s: Number of P-State is less than 2, Fail to do mclk switch \n", __FUNCTION__);
        delete m_pStateSwitch;
        CHECK_RC(RC::SOFTWARE_ERROR);
    }
    return rc;
}

//!
//! Central point where all tests are run.
//!
//! \return OK if test passes, appropriate err code if it fails.
//------------------------------------------------------------------------------
RC GlitchlessMclkSwitchRLTest::Run()
{
   RC rc = OK;
   CHECK_RC(VerifyDisp());
   return rc;
}

RC GlitchlessMclkSwitchRLTest::AllocSetLUTAndGamma(DisplayID Display,
                                      EvoLutSettings *&pLutSettings)
{
    RC rc;
    vector<UINT32>TestLut;
    UINT32 index = 0;
    pLutSettings = new EvoLutSettings();
    EvoDisplay *pEvoDisplay = m_pDisplay->GetEvoDisplay();
    //Set LUT Pallette
    for ( index = 0; index < 256; index++)
    {
        TestLut.push_back( ((UINT32)U8_TO_LUT(index)) << 16 | U8_TO_LUT(index) );  // Green, Red
        TestLut.push_back( (UINT32)U8_TO_LUT(index) );  // Blue
    }
    //257th value = 256th value
    TestLut.push_back( (((UINT32)U8_TO_LUT(index-1)) << 16) | U8_TO_LUT(index-1) );  // Green, Red
    TestLut.push_back( (UINT32)U8_TO_LUT(index-1) );  // Blue
    CHECK_RC(pEvoDisplay->SetupLUTBuffer(Display,
                                         pLutSettings,
                                         &TestLut[0],
                                         (UINT32)TestLut.size()));
    //SetLUT After Base Is Active
    CHECK_RC(pEvoDisplay->SetLUT(Display,
                                 pLutSettings));
    // SET CUSTOM GAMMA
    // 0 -10 .. is the values
    CHECK_RC(pEvoDisplay->SetGamma(Display, pLutSettings, 2, 0, 0));
    return rc;
}

RC GlitchlessMclkSwitchRLTest::FreeLUT(DisplayID Display,
                          EvoLutSettings *pLutSettings)
{
    RC rc;
    EvoLutSettings LutSettings;
    LutSettings.Dirty = false;
    LutSettings.pLutBuffer = NULL;
    CHECK_RC(m_pDisplay->GetEvoDisplay()->SetLUT(Display,
                                 &LutSettings));
    delete pLutSettings;
    pLutSettings = NULL;
    return rc;
}

void GlitchlessMclkSwitchRLTest::setRasterLockMode()
{
    EvoCoreChnContext * pCoreDispContext[NUM_HEAD];
    EvoControlSettings * ControlSettings[NUM_HEAD];
    //master lock on display[0], head 0
    for(int i = 0; i < NUM_HEAD ; i ++)
    {
        pCoreDispContext[i] = (EvoCoreChnContext *)m_pDisplay->GetEvoDisplay()->
        GetEvoDisplayChnContext(m_SelectedDisplay[i], Display::CORE_CHANNEL_ID);
        ControlSettings[i] = &(pCoreDispContext[i]->DispLwstomSettings.HeadSettings[i].ControlSettings);

        ControlSettings[i]->FlipLockEnable = false;
        ControlSettings[i]->FlipLockPin.lockPinType = EvoControlSettings::LOCK_PIN_TYPE_INTERNAL;
        ControlSettings[i]->FlipLockPin.lockPinInternalType = EvoControlSettings::LOCK_PIN_INTERNAL_UNSPECIFIED;
        ControlSettings[i]->FlipLockPin.lockPilwalue = EvoControlSettings::LOCK_PIN_VALUE_UNSPECIFIED;

        ControlSettings[i]->StereoPin.lockPinType = EvoControlSettings::LOCK_PIN_TYPE_INTERNAL;
        ControlSettings[i]->StereoPin.lockPinInternalType = EvoControlSettings::LOCK_PIN_INTERNAL_UNSPECIFIED;
        ControlSettings[i]->StereoPin.lockPilwalue = EvoControlSettings::LOCK_PIN_VALUE_UNSPECIFIED;
        if(i == 0)
        {
            ControlSettings[i]->SlaveLockMode = EvoControlSettings::NO_LOCK;
            ControlSettings[i]->SlaveLockPin.lockPinInternalType = EvoControlSettings::LOCK_PIN_INTERNAL_UNSPECIFIED;
            ControlSettings[i]->SlaveLockPin.lockPilwalue = EvoControlSettings::LOCK_PIN_VALUE_UNSPECIFIED;
            ControlSettings[i]->MasterLockMode = EvoControlSettings::RASTER_LOCK;
            ControlSettings[i]->MasterLockPin.lockPinInternalType = EvoControlSettings::LOCK_PIN_INTERNAL_SCANLOCK;
            ControlSettings[i]->MasterLockPin.lockPilwalue = (EvoControlSettings::LockPilwalue)0;
        }else
        {
            ControlSettings[i]->SlaveLockMode = EvoControlSettings::RASTER_LOCK;
            ControlSettings[i]->SlaveLockPin.lockPinInternalType = EvoControlSettings::LOCK_PIN_INTERNAL_SCANLOCK;
            ControlSettings[i]->SlaveLockPin.lockPilwalue = (EvoControlSettings::LockPilwalue)0;
            ControlSettings[i]->MasterLockMode = EvoControlSettings::NO_LOCK;
            ControlSettings[i]->MasterLockPin.lockPinInternalType = EvoControlSettings::LOCK_PIN_INTERNAL_UNSPECIFIED;
            ControlSettings[i]->MasterLockPin.lockPilwalue = EvoControlSettings::LOCK_PIN_VALUE_UNSPECIFIED;
        }
        ControlSettings[i]->SlaveLockPin.lockPinType = EvoControlSettings::LOCK_PIN_TYPE_INTERNAL;
        ControlSettings[i]->SlaveLockout = 0;
        ControlSettings[i]->MasterLockPin.lockPinType = EvoControlSettings::LOCK_PIN_TYPE_INTERNAL;

        ControlSettings[i]->Dirty = true;
    }
    return;
}

// Test includes
// 1. Initial complex ModeSet (Not Implemented yet) on two Heads
// 2. Read P-states
// 3. Start generate RG CRC
// 4. Switch P-states
// 5. Check UNDERFLOW bit
// 6. Check CRC
// 7. Back to 3.
RC GlitchlessMclkSwitchRLTest::VerifyDisp()
{
    RC rc;
    DisplayIDs Displays;
    UINT32 BaseWidth;
    UINT32 BaseHeight;
    UINT32 OvlyWidth;
    UINT32 OvlyHeight;
    UINT32 LwrsorWidth = 32;
    UINT32 LwrsorHeight = 32;
    UINT32 Depth       = 32;
    UINT32 RefreshRate = 60;
    UINT32 SavedGoldencrc[NUM_HEAD];
    DisplayIDs SelectedDisplays;
    UINT32 isFinishedModeSet = 0;
    //get Detected displays
    CHECK_RC(m_pDisplay->GetDetected(&Displays));
    Printf(Tee::PriHigh, "All Detected displays = \n");
    m_pDisplay->PrintDisplayIds(Tee::PriHigh, Displays);
    /* Select displays
        m_SelectedDisplay[0]  =  RL
        m_SelectedDisplay[1]  =  regular
    */
    // only connected displays
    vector <string> desiredProtocol = DTIUtils::VerifUtils::Tokenize(m_protocol, ",");

    for (UINT32 iter = 0; iter < desiredProtocol.size(); iter++)
    {
        for(UINT32 loopCount = 0; loopCount < Displays.size(); loopCount++)
        {
            for(UINT32 i = 0; i < NUM_HEAD; i ++)
            {
                if( !m_SelectedDisplay[i] && DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay,Displays[loopCount],desiredProtocol[iter]))
                {
                    m_SelectedDisplay[i] = Displays[loopCount];
                    break;
                }
            }
        }
        if(! m_onlyConnectedDisplays)
        {
            // faking  displays
            DisplayIDs supported;
            CHECK_RC(m_pDisplay->GetConnectors(&supported));
            for(UINT32 loopCount = 0; loopCount < supported.size(); loopCount++)
            {
                for(UINT32 i = 0; i < NUM_HEAD; i ++)
                {
                    if (!m_SelectedDisplay[i] && !m_pDisplay->IsDispAvailInList(supported[loopCount], Displays) &&
                        DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay, supported[loopCount], desiredProtocol[iter]))
                    {
                        m_SelectedDisplay[i] = supported[loopCount];
                        CHECK_RC(DTIUtils::EDIDUtils::SetLwstomEdid(m_pDisplay, m_SelectedDisplay[i], false));
                        break;
                    }
                }
            }
        }
        bool allSet = true;
        for(UINT32 i = 0; i < NUM_HEAD; i++)
            if(!m_SelectedDisplay[i])
                allSet = false;
        if(allSet)
            break;
        if(iter == desiredProtocol.size()-1)
        {
            Printf(Tee::PriHigh, "%s: No desired Output Device\n", __FUNCTION__);
            CHECK_RC(RC::SOFTWARE_ERROR);
        }
    }
    if (Platform::GetSimulationMode() == Platform::Hardware)
    {
        Edid *pEdid = m_pDisplay->GetEdidMember();
        pEdid->PrintEdid(m_SelectedDisplay[0]);
        PARSED_EDID   parsedEdid;
        CHECK_RC(pEdid->GetParsedEdid(m_SelectedDisplay[0], &parsedEdid));
        BaseWidth = OvlyWidth = parsedEdid.ModeList[0].mtFront.VVisible;
        BaseHeight = OvlyHeight = parsedEdid.ModeList[0].mtFront.HVisible;
        m_numIter = 30;
    }else{
        BaseWidth = OvlyWidth = 640;
        BaseHeight = OvlyHeight = 480;
        m_numIter = 5;
    }
    if(m_width)
        BaseWidth = OvlyWidth= m_width;
    if(m_height)
        BaseHeight = OvlyHeight= m_height;

    setRasterLockMode();

    Printf(Tee::PriHigh,"%s: Setup Core + Overlay + Lwr + LUT \n", __FUNCTION__);
    for(UINT32 i=0; i<NUM_HEAD; i++)
    {
        CHECK_RC_CLEANUP(m_pDisplay->SetupChannelImage(m_SelectedDisplay[i],
                                             BaseWidth,
                                             BaseHeight,
                                             Depth,
                                             Display::CORE_CHANNEL_ID,
                                             &m_CoreImage[i],
                                             coreChnImageFile));
        CHECK_RC_CLEANUP(m_pDisplay->SetMode(m_SelectedDisplay[i],
                                           BaseWidth,
                                           BaseHeight,
                                           Depth,
                                           RefreshRate));
        CHECK_RC_CLEANUP(m_pDisplay->SetupChannelImage(m_SelectedDisplay[i],
                                             OvlyWidth,
                                             OvlyHeight,
                                             Depth,
                                             Display::OVERLAY_CHANNEL_ID,
                                             &m_OverlayImage[i],
                                             ovlyChnImageFile));
        CHECK_RC_CLEANUP(m_pDisplay->SetupChannelImage(m_SelectedDisplay[i],
                                         LwrsorWidth,
                                         LwrsorHeight,
                                         Depth,
                                         Display::LWRSOR_IMM_CHANNEL_ID,
                                         &m_LwrsorImage[i],
                                         lwrsChnImageFile));
    }
    for(UINT32 i=0; i<NUM_HEAD; i++)
        CHECK_RC_CLEANUP(AllocSetLUTAndGamma(m_SelectedDisplay[i], m_pLutSettings[i]));
    isFinishedModeSet = 1;
    for(UINT32 i=0; i<NUM_HEAD; i++)
        CHECK_RC_CLEANUP(getGoldenCRC(m_SelectedDisplay[i], &SavedGoldencrc[i]));
    CHECK_RC_CLEANUP(runMclkSwitchTest(SavedGoldencrc, m_SelectedDisplay));

Cleanup:
    for(UINT32 i=0; i<NUM_HEAD; i++)
    {
        if(m_CoreImage[i].GetMemHandle() != 0)
            m_CoreImage[i].Free();
        if(m_LwrsorImage[i].GetMemHandle() != 0)
            m_LwrsorImage[i].Free();
        if(m_OverlayImage[i].GetMemHandle() != 0)
            m_OverlayImage[i].Free();
    }
    if(isFinishedModeSet)
    {
        //free
        for(UINT32 i=0; i<NUM_HEAD; i++)
            FreeLUT(m_SelectedDisplay[i], m_pLutSettings[i]);
        m_pDisplay->Selected(&SelectedDisplays);
        if(SelectedDisplays.size() >= 1)
            m_pDisplay->DetachDisplay(SelectedDisplays);
    }
    return rc;
}

RC GlitchlessMclkSwitchRLTest:: checkDispUnderflow()
{
     RC status;
     UINT32    data32;
     UINT32    i;
     for (i = 0; i < NUM_HEAD; i++)
     {
         data32 = GetBoundGpuSubdevice()->RegRd32(LW_PDISP_RG_UNDERFLOW(i));
         if (DRF_VAL( _PDISP, _RG_UNDERFLOW, _UNDERFLOWED, data32))
         {
             Printf(Tee::PriHigh,"%s: Underflow Dected. lw + LW_PDISP_RG_UNDERFLOW(%d)_UNDERFLOWED_YES\n", __FUNCTION__, i);
             status = RC::SOFTWARE_ERROR;
         }
     }
     return status;
}

//! \brief Cleanup
//!
//------------------------------------------------------------------------------
RC GlitchlessMclkSwitchRLTest::Cleanup()
{
    m_pStateSwitch->Cleanup();
    delete m_pStateSwitch;
    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(GlitchlessMclkSwitchRLTest, RmTest,
    "GlitchlessMclkSwitchRLTest test\n");
CLASS_PROP_READWRITE(GlitchlessMclkSwitchRLTest, height, UINT32,
                     "Desired pixel height");
CLASS_PROP_READWRITE(GlitchlessMclkSwitchRLTest, width, UINT32,
                     "Desired pixel width");
CLASS_PROP_READWRITE(GlitchlessMclkSwitchRLTest, protocol, string,
                     "OR protocol to use for the test");
CLASS_PROP_READWRITE(GlitchlessMclkSwitchRLTest,onlyConnectedDisplays,bool,
                     "run on only connected displays, default = 0");

