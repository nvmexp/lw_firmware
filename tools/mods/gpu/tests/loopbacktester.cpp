/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Display SOR loopback test.
// Based on //arch/traces/non3d/iso/disp/class010X/core/disp_core_sor_loopback_vaio/test.js
//          //arch/traces/ip/display/3.0/branch_trace/full_chip/sor_loopback/src/2.0/test.cpp
#include "loopbacktester.h"
#include "core/include/modsedid.h"
#include "core/include/platform.h"
#include "core/include/xp.h"
#include "core/include/jscript.h"
#include "Lwcm.h"
#include "core/include/utility.h"
#include "gpu/include/displaycleanup.h"
#include "gpu/utility/scopereg.h"

#include "class/cl8870.h"
#include "class/cl9070.h"
#include "class/cl9270.h"
#include "class/cl9470.h"
#include "class/cl9770.h"
#include "class/cl507d.h" // Core channel
#include "class/cl907d.h" // Core channel
#include "class/clc570.h"

#include "gpu/display/evo_disp.h"
#include "gpu/display/lwdisplay/lw_disp.h"
#include "gpu/display/lwdisplay/lw_disp_crc_handler_c3.h"

#define ILWALID_PADLINK_NUM -1
//------------------------------------------------------------------------------
// LoopbackTester
//------------------------------------------------------------------------------
// TMDS Patterns
//   General,      10-02-04-8-1, 02-04-77-3f,  10-02-04-8-1, half-clock,   max-stress
const UINT64 LoopbackTester::DebugData[NUM_PATTERNS] =
    {0x0a8771b803, 0x0200400810, 0x0fc7710020, 0xaabf7ff803, 0xf83e0f83e0, 0x5555555555};

// LVDS Patterns
//   General,      55-55-55-55-55, 02-04-77-3f-fe, 10-02-04-08-01, 03-6e-77-2s-55, max_stress
const UINT64 LegacyLoopbackTester::DebugDataLVDS[NUM_PATTERNS] =
    {0xaaaaaaaaaa, 0x055ab56ad5,   0x07e7fde020,   0x0011010110,   0x05555df703,   0x5555555555};
const UINT64 LegacyLoopbackTester::EncodedDataLVDS[NUM_PATTERNS] =
    {0x0525b496d2, 0x02d5ab56ad,   0x077f6fc202,   0x0088080801,   0x02da4ffb18,   0x02da4b692d};
const UINT64 LegacyLoopbackTester::CrcsLVDS[] =
    {0xb0eac93bae54, 0xbadb3924ffd5, 0x0a31a79654c4, 0x0000affd935a, 0xbadbdcdf8740, 0xbadbb948de27};

LoopbackTester::LoopbackTester
(
    GpuSubdevice *pSubdev,
    Display *display,
    const SorLoopbackTestArgs& sorLoopbackTestArgs,
    Tee::Priority printPriority
)
: m_pSubdev(pSubdev)
, m_pDisplay(display)
, m_SorLoopbackTestArgs(sorLoopbackTestArgs)
, m_PrintPriority(printPriority)
{
    m_LwDispRegHal = make_unique<LwDispUtils::LwDispRegHal>(display);
}

RC LoopbackTester::RunLoopbackTest
(
    const DisplayID &displayID,
    UINT32 SorNumber,
    UINT32 Protocol,
    Display::Encoder::Links link,
    UPHY_LANES lane
)
{
    StickyRC rc;
    {
        DEFER
        {
            rc = DisableSORDebug(SorNumber);
            rc = RestoreRegisters(SorNumber);
        };

        CHECK_RC(SaveRegisters(SorNumber));

        bool isProtocolTMDS = false;
        bool isLinkATestNeeded = false;
        bool isLinkBTestNeeded = false;

        CHECK_RC(GetLinksUnderTestInfo(SorNumber, Protocol, &isProtocolTMDS,
            &isLinkATestNeeded, &isLinkBTestNeeded));

        if (isProtocolTMDS)
        {
            CHECK_RC(RecalcEncodedDataAndCrcs(SorNumber));
        }
        else
        {
            CHECK_RC(EnableLVDS(SorNumber));
        }

        CHECK_RC(WaitForNormalMode(SorNumber));

        if (m_DoWarsCB)
        {
            CHECK_RC(m_DoWarsCB(SorNumber, isProtocolTMDS, isLinkATestNeeded,
                    isLinkBTestNeeded));
        }
        CHECK_RC(ProgramLoadPulsePositionAdjust(SorNumber,
            m_SorLoopbackTestArgs.pllLoadAdj, m_SorLoopbackTestArgs.pllLoadAdj));

        CHECK_RC(EnableSORDebug(SorNumber, lane));

        CHECK_RC(RunLoopbackTestInner(SorNumber, isProtocolTMDS,
            isLinkATestNeeded, isLinkBTestNeeded, lane));
    }
    return rc;
}

RC LoopbackTester::SaveRegisters(UINT32 SorNumber)
{
    RegHal &regs = m_pSubdev->Regs();

    m_SaveSorTrig = m_LwDispRegHal->Read32(MODS_PDISP_SOR_TRIG, SorNumber);
    m_SaveSorTest = m_LwDispRegHal->Read32(MODS_PDISP_SOR_TEST, SorNumber);

    m_SaveSorDebugA = RegPairRd64(regs.LookupAddress(MODS_PDISP_SOR_DEBUGA0, SorNumber));
    m_SaveSorDebugB = RegPairRd64(regs.LookupAddress(MODS_PDISP_SOR_DEBUGB0, SorNumber));

    vector<ModsGpuRegField> loadAdjRegsFields = GetLoadAdjRegsFields(regs);
    m_SaveLoadAdjA = m_LwDispRegHal->Read32(loadAdjRegsFields[0], SorNumber);
    if (loadAdjRegsFields.size() == 2)
    {
        m_SaveLoadAdjB = m_LwDispRegHal->Read32(loadAdjRegsFields[1], SorNumber);
    }
    m_SaveSorLvds = m_LwDispRegHal->Read32(MODS_PDISP_SOR_LVDS, SorNumber);

    return OK;
}

RC LoopbackTester::RestoreRegisters(UINT32 SorNumber)
{
    RC rc;
    RegHal &regs = m_pSubdev->Regs();
    m_LwDispRegHal->Write32(MODS_PDISP_SOR_LVDS, SorNumber, m_SaveSorLvds);

    CHECK_RC(ProgramLoadPulsePositionAdjust(SorNumber, m_SaveLoadAdjA, m_SaveLoadAdjB));

    RegPairWr64(regs.LookupAddress(MODS_PDISP_SOR_DEBUGA0, SorNumber), m_SaveSorDebugA);
    RegPairWr64(regs.LookupAddress(MODS_PDISP_SOR_DEBUGB0, SorNumber), m_SaveSorDebugB);

    m_LwDispRegHal->Write32(MODS_PDISP_SOR_TEST, SorNumber, m_SaveSorTest);
    m_LwDispRegHal->Write32(MODS_PDISP_SOR_TRIG, SorNumber, m_SaveSorTrig);

    return rc;
}

RC LoopbackTester::EnableSORDebug(UINT32 sorNumber, UPHY_LANES lane)
{
    RegHal &regs = m_pSubdev->Regs();

    m_LwDispRegHal->Write32(MODS_PDISP_SOR_TRIG, sorNumber, 0x4);
    m_LwDispRegHal->Write32(MODS_PDISP_SOR_TEST_DSRC_DEBUG, sorNumber);

    if (regs.IsSupported(MODS_PDISP_SOR_TEST_DEEP_LOOPBK_ENABLE))
    {
        m_LwDispRegHal->Write32(MODS_PDISP_SOR_TEST_DEEP_LOOPBK_ENABLE, sorNumber);
    }

    return OK;
}

RC LoopbackTester::DisableSORDebug(UINT32 sorNumber)
{
    //clear TEST_DSRC so the afifo will be stopped.
    m_pSubdev->Regs().Write32(MODS_PDISP_SOR_TEST, sorNumber,
            m_pSubdev->Regs().SetField(MODS_PDISP_SOR_TEST_DSRC_NORMAL));

    return OK;
}

RC LoopbackTester::GetLinksUnderTestInfo
(
    UINT32 sorNumber,
    UINT32 protocol,
    bool *pIsProtocolTMDS,
    bool *pIsLinkATestNeeded,
    bool *pIsLinkBTestNeeded
)
{
    RC rc;

    *pIsProtocolTMDS = protocol != LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_LVDS_LWSTOM;

    *pIsLinkATestNeeded = (protocol == LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DUAL_TMDS) ||
                          (protocol == LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_SINGLE_TMDS_A) ||
                          (!*pIsProtocolTMDS && m_SorLoopbackTestArgs.lvdsA);

    *pIsLinkBTestNeeded = (protocol == LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DUAL_TMDS) ||
                          (protocol == LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_SINGLE_TMDS_B) ||
                          (!*pIsProtocolTMDS && m_SorLoopbackTestArgs.lvdsB);

    if (!*pIsLinkATestNeeded && !*pIsLinkBTestNeeded)
    {
        Printf(Tee::PriError, "SorLoopback error: SOR%d - Link selection failed.\n", sorNumber);
        return RC::SOFTWARE_ERROR;
    }

    VerbosePrintf("Links tested:%s%s\n",
            *pIsLinkATestNeeded ? " First" : "",
            *pIsLinkBTestNeeded ? " Second" :  "");

    return rc;
}

RC LoopbackTester::EnableLVDS(UINT32 sorNumber)
{
    // Enable all links:
    RegHal &regs = m_pSubdev->Regs();
    UINT32 SorLvds = m_LwDispRegHal->Read32(MODS_PDISP_SOR_LVDS, sorNumber);
    regs.SetField(&SorLvds, MODS_PDISP_SOR_LVDS_PD_TXDA_0_ENABLE);
    regs.SetField(&SorLvds, MODS_PDISP_SOR_LVDS_PD_TXDA_1_ENABLE);
    regs.SetField(&SorLvds, MODS_PDISP_SOR_LVDS_PD_TXDA_2_ENABLE);
    regs.SetField(&SorLvds, MODS_PDISP_SOR_LVDS_PD_TXDA_3_ENABLE);
    regs.SetField(&SorLvds, MODS_PDISP_SOR_LVDS_PD_TXDB_0_ENABLE);
    regs.SetField(&SorLvds, MODS_PDISP_SOR_LVDS_PD_TXDB_1_ENABLE);
    regs.SetField(&SorLvds, MODS_PDISP_SOR_LVDS_PD_TXDB_2_ENABLE);
    regs.SetField(&SorLvds, MODS_PDISP_SOR_LVDS_PD_TXDB_3_ENABLE);
    regs.SetField(&SorLvds, MODS_PDISP_SOR_LVDS_PD_TXCA_ENABLE);
    regs.SetField(&SorLvds, MODS_PDISP_SOR_LVDS_PD_TXCB_ENABLE);
    m_LwDispRegHal->Write32(MODS_PDISP_SOR_LVDS, sorNumber, SorLvds);

    return OK;
}

RC LoopbackTester::WaitForNormalMode(UINT32 sorNumber)
{
    RegHal &regs = m_pSubdev->Regs();
    // Allow test to proceed even for failure - so as to analyze the complete flow
    PollRegValue(regs.LookupAddress(MODS_PDISP_SOR_PWR, sorNumber),
                 regs.SetField(MODS_PDISP_SOR_PWR_MODE_NORMAL),
                 regs.LookupMask(MODS_PDISP_SOR_PWR_MODE),
                 m_SorLoopbackTestArgs.timeoutMs);

    return OK;
}

RC LoopbackTester::ProgramLoadPulsePositionAdjust
(
    UINT32 sorNumber,
    UINT32 loadAdj0,
    UINT32 loadAdj1
)
{
    RegHal &regs = m_pSubdev->Regs();

    if (m_SorLoopbackTestArgs.pllLoadAdjOverride)
    {
        vector<ModsGpuRegField> loadAdjRegsFields = GetLoadAdjRegsFields(regs);
        m_LwDispRegHal->Write32(loadAdjRegsFields[0], sorNumber, m_SorLoopbackTestArgs.pllLoadAdj);
        if (loadAdjRegsFields.size() == 2)
        {
            m_LwDispRegHal->Write32(loadAdjRegsFields[1], sorNumber,
                                        m_SorLoopbackTestArgs.pllLoadAdj);
        }
    }

    return OK;
}

vector<ModsGpuRegField> LoopbackTester::GetLoadAdjRegsFields(RegHal &regs)
{
    vector<ModsGpuRegField> loadAdjRegsFields;
    if (regs.IsSupported(MODS_PDISP_SOR_PLL1_LOADADJ))
    {
        //pre-pascal location of loadadj
        loadAdjRegsFields.push_back(MODS_PDISP_SOR_PLL1_LOADADJ);
    }
    else
    {
        //pascal+ location of loadadj
        loadAdjRegsFields.push_back(MODS_PDISP_SOR_DP_PADCTL0_LOADADJ);
        loadAdjRegsFields.push_back(MODS_PDISP_SOR_DP_PADCTL1_LOADADJ);
    }

    return loadAdjRegsFields;
}

RC LoopbackTester::RunLoopbackTestInner
(
    UINT32 sorNumber,
    bool isProtocolTMDS,
    bool isLinkATestNeeded,
    bool isLinkBTestNeeded,
    UPHY_LANES lane
)
{
    RC rc;

    RegHal &regs = m_pSubdev->Regs();
    DEFER
    {
        if (m_SorLoopbackTestArgs.finalZeroPatternMS)
        {
            RegPairWr64(regs.LookupAddress(MODS_PDISP_SOR_DEBUGA0, sorNumber), 0);
            RegPairWr64(regs.LookupAddress(MODS_PDISP_SOR_DEBUGB0, sorNumber), 0);
            Tasker::Sleep(m_SorLoopbackTestArgs.finalZeroPatternMS);
        }

        RestoreRegisters(sorNumber);
    };

    if (m_SorLoopbackTestArgs.initialZeroPatternMS)
    {
        RegPairWr64(regs.LookupAddress(MODS_PDISP_SOR_DEBUGA0, sorNumber), 0);
        RegPairWr64(regs.LookupAddress(MODS_PDISP_SOR_DEBUGB0, sorNumber), 0);
        Tasker::Sleep(m_SorLoopbackTestArgs.initialZeroPatternMS);
    }

    for (UINT32 patternIndex = 0; patternIndex < NUM_PATTERNS; patternIndex++)
    {
        if (m_SorLoopbackTestArgs.forcePattern &&
                (patternIndex != m_SorLoopbackTestArgs.forcedPatternIdx))
            continue;

        CHECK_RC(WritePatternAndAwaitEData(sorNumber, patternIndex, isProtocolTMDS,
            isLinkATestNeeded, isLinkBTestNeeded, lane));

        // Bug 279674:
        if (m_SorLoopbackTestArgs.crcSleepMS != 0)
            Tasker::Sleep(m_SorLoopbackTestArgs.crcSleepMS);

        if (m_SorLoopbackTestArgs.enableDisplayRegistersDump)
        {
            m_pDisplay->DumpDisplayRegisters(GetVerbosePrintPri());
        }

        CHECK_RC(MatchCrcs(sorNumber, patternIndex, isProtocolTMDS,
            isLinkATestNeeded, isLinkBTestNeeded, lane));
    }

    return rc;
}

void LoopbackTester::DumpSorInfoOnCrcMismatch(UINT32 sorNumber)
{
    RegHal &regs = m_pSubdev->Regs();
    if (m_SorLoopbackTestArgs.printErrorDetails)
    {

        Printf(Tee::PriNormal, "LW_PDISP_SOR_PLL0(%d) = 0x%08x.\n",
            sorNumber, m_LwDispRegHal->Read32(MODS_PDISP_SOR_PLL0, sorNumber));
        Printf(Tee::PriNormal, "LW_PDISP_SOR_PLL1(%d) = 0x%08x.\n",
            sorNumber, m_LwDispRegHal->Read32(MODS_PDISP_SOR_PLL1, sorNumber));
        Printf(Tee::PriNormal, "LW_PDISP_SOR_PLL2(%d) = 0x%08x.\n",
            sorNumber, m_LwDispRegHal->Read32(MODS_PDISP_SOR_PLL2, sorNumber));
        Printf(Tee::PriNormal, "LW_PDISP_SOR_CSTM(%d) = 0x%08x.\n",
            sorNumber, m_LwDispRegHal->Read32(MODS_PDISP_SOR_CSTM, sorNumber));

        Printf(Tee::PriNormal, "LW_PDISP_SOR_EDATAA(%d) = 0x%016llx.\n", sorNumber,
                RegPairRd64(regs.LookupAddress(MODS_PDISP_SOR_EDATAA0, sorNumber)));
        Printf(Tee::PriNormal, "LW_PDISP_SOR_EDATAB(%d) = 0x%016llx.\n", sorNumber,
                RegPairRd64(regs.LookupAddress(MODS_PDISP_SOR_EDATAB0, sorNumber)));
    }

    if (m_SorLoopbackTestArgs.sleepMSOnError != 0)
    {
        Tasker::Sleep(m_SorLoopbackTestArgs.sleepMSOnError);
    }

    if (m_SorLoopbackTestArgs.enableDisplayRegistersDumpOnCrcMiscompare)
    {
        m_pDisplay->DumpDisplayRegisters(GetVerbosePrintPri());
    }
}

void LoopbackTester::GenerateCRC(UINT32 dataIn, UINT32 *crcOut0, UINT32 *crcOut1)
{
    MASSERT(crcOut0);
    MASSERT(crcOut1);
    BitSet16 cIn;
    BitSet16 cOut;
    BitSet16 dIn(dataIn);

    const UINT32 numChannels = 5;
    for (UINT32 i = 0; i < numChannels; i++)
    {
        CRCOperations(&cIn, &dIn, &cOut);
        cIn = cOut;
    }

    *crcOut0 = cOut.to_ulong();

    BitSet16 dIn1(dataIn >> 16);
    cIn.reset();
    for (UINT32 i = 0; i < numChannels; i++)
    {
        CRCOperations(&cIn, &dIn1, &cOut);
        cIn = cOut;
    }

    *crcOut1 = cOut.to_ulong();
}

void LoopbackTester::CRCOperations(BitSet16 *cIn, BitSet16 *dIn, BitSet16 *cOut)
{
    MASSERT(cIn);
    MASSERT(dIn);
    MASSERT(cOut);

    (*cOut)[15] = (*cIn)[15] ^ (*dIn)[15] ^ (*cIn)[10] ^ (*dIn)[10] ^ (*cIn)[5] ^
                  (*dIn)[5] ^ (*cIn)[3] ^ (*dIn)[3] ^ (*cIn)[0] ^ (*dIn)[0];

    (*cOut)[14] = (*cIn)[14] ^ (*dIn)[14] ^ (*cIn)[9] ^ (*dIn)[9] ^ (*cIn)[4] ^
                  (*dIn)[4] ^ (*cIn)[2] ^ (*dIn)[2];

    (*cOut)[13] = (*cIn)[13] ^ (*dIn)[13] ^ (*cIn)[8] ^ (*dIn)[8] ^ (*cIn)[3] ^
                  (*dIn)[3] ^ (*cIn)[1] ^ (*dIn)[1];

    (*cOut)[12] = (*cIn)[12] ^ (*dIn)[12] ^ (*cIn)[7] ^ (*dIn)[7] ^ (*cIn)[2] ^
                  (*dIn)[2] ^ (*cIn)[0] ^ (*dIn)[0];

    (*cOut)[11] = (*cIn)[15] ^ (*dIn)[15] ^ (*cIn)[11] ^ (*dIn)[11] ^ (*cIn)[10] ^
                  (*dIn)[10] ^ (*cIn)[6] ^ (*dIn)[6] ^ (*cIn)[5] ^ (*dIn)[5] ^
                  (*cIn)[3] ^ (*dIn)[3] ^ (*cIn)[1] ^ (*dIn)[1] ^ (*cIn)[0] ^ (*dIn)[0];

    (*cOut)[10] = (*cIn)[14] ^ (*dIn)[14] ^ (*cIn)[10] ^ (*dIn)[10] ^ (*cIn)[9] ^
                  (*dIn)[9] ^ (*cIn)[5] ^ (*dIn)[5] ^ (*cIn)[4] ^ (*dIn)[4] ^
                  (*cIn)[2] ^ (*dIn)[2] ^ (*cIn)[0] ^ (*dIn)[0];

    (*cOut)[9] = (*cIn)[13] ^ (*dIn)[13] ^ (*cIn)[9] ^ (*dIn)[9] ^ (*cIn)[8] ^ (*dIn)[8] ^
                 (*cIn)[4] ^ (*dIn)[4] ^ (*cIn)[3] ^ (*dIn)[3] ^ (*cIn)[1] ^ (*dIn)[1];

    (*cOut)[8] = (*cIn)[12] ^ (*dIn)[12] ^ (*cIn)[8] ^ (*dIn)[8] ^ (*cIn)[7] ^ (*dIn)[7] ^
                 (*cIn)[3] ^ (*dIn)[3] ^ (*cIn)[2] ^ (*dIn)[2] ^ (*cIn)[0] ^ (*dIn)[0];

    (*cOut)[7] = (*cIn)[11] ^ (*dIn)[11] ^ (*cIn)[7] ^ (*dIn)[7] ^ (*cIn)[6] ^
                 (*dIn)[6] ^ (*cIn)[2] ^ (*dIn)[2] ^ (*cIn)[1] ^ (*dIn)[1];

    (*cOut)[6] = (*cIn)[10] ^ (*dIn)[10] ^ (*cIn)[6] ^ (*dIn)[6] ^ (*cIn)[5] ^
                 (*dIn)[5] ^ (*cIn)[1] ^ (*dIn)[1] ^ (*cIn)[0] ^ (*dIn)[0];

    (*cOut)[5] = (*cIn)[9] ^ (*dIn)[9] ^ (*cIn)[5] ^ (*dIn)[5] ^ (*cIn)[4] ^
                 (*dIn)[4] ^ (*cIn)[0] ^ (*dIn)[0];

    (*cOut)[4] = (*cIn)[15] ^ (*dIn)[15] ^ (*cIn)[10] ^ (*dIn)[10] ^ (*cIn)[8] ^ (*dIn)[8] ^
                 (*cIn)[5] ^ (*dIn)[5] ^ (*cIn)[4] ^ (*dIn)[4] ^ (*cIn)[0] ^ (*dIn)[0];

    (*cOut)[3] = (*cIn)[14] ^ (*dIn)[14] ^ (*cIn)[9] ^ (*dIn)[9] ^ (*cIn)[7] ^
                 (*dIn)[7] ^ (*cIn)[4] ^ (*dIn)[4] ^ (*cIn)[3] ^ (*dIn)[3];

    (*cOut)[2] = (*cIn)[13] ^ (*dIn)[13] ^ (*cIn)[8] ^ (*dIn)[8] ^ (*cIn)[6] ^
                 (*dIn)[6] ^ (*cIn)[3] ^ (*dIn)[3] ^ (*cIn)[2] ^ (*dIn)[2];

    (*cOut)[1] = (*cIn)[12] ^ (*dIn)[12] ^ (*cIn)[7] ^ (*dIn)[7] ^ (*cIn)[5] ^
                 (*dIn)[5] ^ (*cIn)[2] ^ (*dIn)[2] ^ (*cIn)[1] ^ (*dIn)[1];

    (*cOut)[0] = (*cIn)[11] ^ (*dIn)[11] ^ (*cIn)[6] ^ (*dIn)[6] ^ (*cIn)[4] ^
                 (*dIn)[4] ^ (*cIn)[1] ^ (*dIn)[1] ^ (*cIn)[0] ^ (*dIn)[0];
}

INT32 LoopbackTester::VerbosePrintf(const char* format, ...) const
{
    va_list args;
    va_start(args, format);

    const INT32 charsWritten = ModsExterlwAPrintf(GetVerbosePrintPri(),
        Tee::GetTeeModuleCoreCode(), Tee::SPS_NORMAL, format, args);

    va_end(args);
    return charsWritten;
}

UINT64 LoopbackTester::RegPairRd64(UINT32 offset)
{
    return m_pSubdev->RegRd32(offset) |
        (static_cast<UINT64>(m_pSubdev->RegRd32(offset+4)) << 32);
}

void LoopbackTester::RegPairWr64
(
    UINT32 regAddr,
    UINT64 data
)
{
    m_pSubdev->RegWr32(regAddr, LwU64_LO32(data));
    m_pSubdev->RegWr32(regAddr+4, LwU64_HI32(data));
}

bool LoopbackTester::GpuPollRegValue
(
   void * Args
)
{
    PollRegValue_Args * pArgs = static_cast<PollRegValue_Args*>(Args);

    UINT32 RegValue = pArgs->Subdev->RegRd32(pArgs->RegAdr);
    RegValue &= pArgs->DataMask;
    return (RegValue == pArgs->ExpectedData);
}

RC LoopbackTester::PollPairRegValue
(
    UINT32 regAddr,
    UINT64 expectedData,
    UINT64 availableBits,
    FLOAT64 timeoutMs
)
{
    RC rc;

    UINT32 lowerRegExpectedData = LwU64_LO32(expectedData);
    CHECK_RC(PollRegValue(regAddr, lowerRegExpectedData, 0xFFFFFFFF, timeoutMs));

    UINT32 upperRegExpectedData = LwU64_HI32(expectedData);
    CHECK_RC(PollRegValue(regAddr + 4, upperRegExpectedData,
        ~(1 << (availableBits-32)), timeoutMs));

    return rc;
}

RC LoopbackTester::PollRegValue
(
    UINT32 regAddr,
    UINT32 expectedData,
    UINT32 dataMask,
    FLOAT64 timeoutMs
)
{
    RC rc;

    PollRegValue_Args Args =
    {
        regAddr,
        expectedData & dataMask,
        dataMask,
        m_pSubdev
    };

    rc = POLLWRAP_HW(LoopbackTester::GpuPollRegValue, &Args, timeoutMs);

    if (rc != RC::OK)
        VerbosePrintf(
            "SorLoopback: PollRegValue error %d,"
            " Register 0x%08x: 0x%08x != expected 0x%08x.\n",
            rc.Get(), regAddr,
            m_pSubdev->RegRd32(regAddr) & dataMask, expectedData & dataMask);

    return rc;
}

RC LoopbackTester::IsTestRequiredForDisplay
(
    UINT32 loopIndex,
    const DisplayID& displayID,
    bool *pIsRequired
)
{
    RC rc;

    MASSERT(pIsRequired);
    *pIsRequired = false;
    do
    {
        if (displayID & m_SorLoopbackTestArgs.skipDisplayMask)
        {
            VerbosePrintf("SorLoopback: Skipping display 0x%x (user override)\n",
                displayID.Get());
            break;
        }
        if (!LwDispUtils::IsAtleastOrin())
        {
            UINT32 virtualDisplayMask = 0;
            CHECK_RC(m_pDisplay->GetVirtualDisplays(&virtualDisplayMask));
            if (virtualDisplayMask & displayID)
            {
                VerbosePrintf("SorLoopback: Skipping display 0x%x (virtual)\n",
                    displayID.Get());
                break;
            }
        }

        if (m_IsTestRequiredForDisplayCB)
        {
            if (!m_IsTestRequiredForDisplayCB(loopIndex, displayID))
            {
                break;
            }
        }

        *pIsRequired = true;
        UINT32 orType = LW0073_CTRL_SPECIFIC_OR_TYPE_NONE;
        UINT32 protocol;
        rc = m_pDisplay->GetORProtocol(displayID, nullptr, &orType, &protocol); 
        if ((rc == RC::OK) &&
            (orType != LW0073_CTRL_SPECIFIC_OR_TYPE_SOR))
        {
            *pIsRequired = false;
        }
        else if (m_FrlMode && (protocol != LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_SINGLE_TMDS_A))
        {
            *pIsRequired = false;
        }
        else if (m_pDisplay->GetDisplayType(displayID) == Display::DFP)
        {
            CHECK_RC(IsSupportedSignalType(displayID, pIsRequired));
        }

        if (!*pIsRequired)
        {
            VerbosePrintf("Display 0x%.8x is of unsupported kind, skipping.\n", displayID.Get());
            break;
        }
    } while(0);

    return RC::OK;
}

RC LoopbackTester::IsSupportedSignalType(const DisplayID& displayID, bool *pIsSupported)
{
    RC rc;

    *pIsSupported = true;

    LW0073_CTRL_DFP_GET_INFO_PARAMS dfpInfo = {0};
    dfpInfo.subDeviceInstance = m_pSubdev->GetSubdeviceInst();
    dfpInfo.displayId = displayID;
    CHECK_RC(m_pDisplay->RmControl(LW0073_CTRL_CMD_DFP_GET_INFO,
        &dfpInfo, sizeof(dfpInfo)));

    switch (REF_VAL(LW0073_CTRL_DFP_FLAGS_SIGNAL, dfpInfo.flags))
    {
        case LW0073_CTRL_DFP_FLAGS_SIGNAL_LVDS:
            if (m_SorLoopbackTestArgs.skipLVDS)
            {
                Printf(Tee::PriWarn, "Skipping LVDS.\n");
                *pIsSupported = false;
            }
            break;
        case LW0073_CTRL_DFP_FLAGS_SIGNAL_SDI:
        case LW0073_CTRL_DFP_FLAGS_SIGNAL_DISPLAYPORT:
        case LW0073_CTRL_DFP_FLAGS_SIGNAL_DSI:
            *pIsSupported = false;
            break;
        case LW0073_CTRL_DFP_FLAGS_SIGNAL_TMDS:
            break;
        default:
            Printf(Tee::PriError, "Sorloopback: Invalid Signal type.\n");
            return RC::HW_STATUS_ERROR;
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
// LegacyLoopbackTester
//------------------------------------------------------------------------------
LegacyLoopbackTester::LegacyLoopbackTester
(
    GpuSubdevice *pSubdev,
    Display *display,
    const SorLoopbackTestArgs& sorLoopbackTestArgs,
    Tee::Priority printPriority
)
:LoopbackTester(pSubdev, display, sorLoopbackTestArgs, printPriority)
{

}

// TMDS lane swizzling
RC LegacyLoopbackTester::RecalcEncodedDataAndCrcs(UINT32 SorNumber)
{
    RC rc;
    const UINT32 numChannels = 5; // Number of TMDS channels per link
    RegHal &regs = m_pSubdev->Regs();
    // Read and decode LW_PDISP_SOR_XBAR_CTRL
    UINT32 xbarCtrl = m_LwDispRegHal->Read32(MODS_PDISP_SOR_XBAR_CTRL, SorNumber);

    UINT32 linkAXsel[numChannels] = {0};
    linkAXsel[0] = regs.GetField(xbarCtrl, MODS_PDISP_SOR_XBAR_CTRL_LINK0_XSEL_0);
    linkAXsel[1] = regs.GetField(xbarCtrl, MODS_PDISP_SOR_XBAR_CTRL_LINK0_XSEL_1);
    linkAXsel[2] = regs.GetField(xbarCtrl, MODS_PDISP_SOR_XBAR_CTRL_LINK0_XSEL_2);
    linkAXsel[3] = regs.GetField(xbarCtrl, MODS_PDISP_SOR_XBAR_CTRL_LINK0_XSEL_3);
    linkAXsel[4] = regs.GetField(xbarCtrl, MODS_PDISP_SOR_XBAR_CTRL_LINK0_XSEL_4);

    UINT32 linkBXsel[numChannels] = {0};
    linkBXsel[0] = regs.GetField(xbarCtrl, MODS_PDISP_SOR_XBAR_CTRL_LINK1_XSEL_0);
    linkBXsel[1] = regs.GetField(xbarCtrl, MODS_PDISP_SOR_XBAR_CTRL_LINK1_XSEL_1);
    linkBXsel[2] = regs.GetField(xbarCtrl, MODS_PDISP_SOR_XBAR_CTRL_LINK1_XSEL_2);
    linkBXsel[3] = regs.GetField(xbarCtrl, MODS_PDISP_SOR_XBAR_CTRL_LINK1_XSEL_3);    
    linkBXsel[4] = regs.GetField(xbarCtrl, MODS_PDISP_SOR_XBAR_CTRL_LINK1_XSEL_4);

    // Read and decode LW_PDISP_SOR_XBAR_POL
    UINT32 polSel = m_LwDispRegHal->Read32(MODS_PDISP_SOR_XBAR_POL, SorNumber);

    UINT32 linkAPol[numChannels]  = {0};
    linkAPol[0] = regs.GetField(polSel, MODS_PDISP_SOR_XBAR_POL_LINK0_0);
    linkAPol[1] = regs.GetField(polSel, MODS_PDISP_SOR_XBAR_POL_LINK0_1);
    linkAPol[2] = regs.GetField(polSel, MODS_PDISP_SOR_XBAR_POL_LINK0_2);
    linkAPol[3] = regs.GetField(polSel, MODS_PDISP_SOR_XBAR_POL_LINK0_3);
    linkAPol[4] = regs.GetField(polSel, MODS_PDISP_SOR_XBAR_POL_LINK0_4);

    UINT32 linkBPol[numChannels]  = {0};
    linkBPol[0] = regs.GetField(polSel, MODS_PDISP_SOR_XBAR_POL_LINK1_0);
    linkBPol[1] = regs.GetField(polSel, MODS_PDISP_SOR_XBAR_POL_LINK1_1);
    linkBPol[2] = regs.GetField(polSel, MODS_PDISP_SOR_XBAR_POL_LINK1_2);
    linkBPol[3] = regs.GetField(polSel, MODS_PDISP_SOR_XBAR_POL_LINK1_3);
    linkBPol[4] = regs.GetField(polSel, MODS_PDISP_SOR_XBAR_POL_LINK1_4);

    bool bypassXbar = regs.TestField(xbarCtrl, MODS_PDISP_SOR_XBAR_CTRL_BYPASS_INIT);
    bool swapLink   = !regs.TestField(xbarCtrl, MODS_PDISP_SOR_XBAR_CTRL_LINK_SWAP_INIT);

    if (bypassXbar)
    {
        // set the crossbar settings to default so that the crc callwlation
        // code will work correctly
        for (UINT32 i = 0; i < numChannels; i++)
        {
            linkAXsel[i] = i;
            linkBXsel[i] = i;
        }
        linkAPol[0] = regs.LookupValue(MODS_PDISP_SOR_XBAR_POL_LINK0_0_NORMAL);
        linkAPol[1] = regs.LookupValue(MODS_PDISP_SOR_XBAR_POL_LINK0_1_NORMAL);
        linkAPol[2] = regs.LookupValue(MODS_PDISP_SOR_XBAR_POL_LINK0_2_NORMAL);
        linkAPol[3] = regs.LookupValue(MODS_PDISP_SOR_XBAR_POL_LINK0_3_NORMAL);
        linkAPol[4] = regs.LookupValue(MODS_PDISP_SOR_XBAR_POL_LINK0_4_NORMAL);

        linkBPol[0] = regs.LookupValue(MODS_PDISP_SOR_XBAR_POL_LINK0_0_NORMAL);
        linkBPol[1] = regs.LookupValue(MODS_PDISP_SOR_XBAR_POL_LINK0_1_NORMAL);
        linkBPol[2] = regs.LookupValue(MODS_PDISP_SOR_XBAR_POL_LINK0_2_NORMAL);
        linkBPol[3] = regs.LookupValue(MODS_PDISP_SOR_XBAR_POL_LINK0_3_NORMAL);
        linkBPol[4] = regs.LookupValue(MODS_PDISP_SOR_XBAR_POL_LINK0_4_NORMAL);
    }
    // Update TMDS expected patterns
    UINT32 preLinkAData[numChannels][NUM_PATTERNS] = {{}};
    UINT32 preLinkBData[numChannels][NUM_PATTERNS] = {{}};

    for (UINT32 i = 0; i < numChannels; i++)
    {
        for (UINT32 j = 0; j < NUM_PATTERNS; j++)
        {
            // SOR1 only has 4 lanes per link, so L4 gets L3 data
            UINT32 laneNumToUseForLinkA = (linkAXsel[i] == 0x4) ? 0x3 : linkAXsel[i];

            preLinkAData[i][j] = (DebugData[j] & (0x3FF << (laneNumToUseForLinkA * 10)))
                >> (laneNumToUseForLinkA * 10);

            UINT32 laneNumToUseForLinkB = (linkBXsel[i] == 0x4) ? 0x3 : linkBXsel[i];
            preLinkBData[i][j] = (DebugData[j] & (0x3FF << (laneNumToUseForLinkB * 10)))
                >> (laneNumToUseForLinkB * 10);

            // Ilwert
            if (linkAPol[i])
                preLinkAData[i][j] = (preLinkAData[i][j] ^ 0x3FF) & 0x3FF;

            // Ilwert
            if (linkBPol[i])
                preLinkBData[i][j] = (preLinkBData[i][j] ^ 0x3FF) & 0x3FF;
        }
    }

    // On SOR0 there are 5 lanes per link
    // but on SOR1 there are only 4 lanes per link
    // therefore lane4's data is sent to lane3
    const UINT32 lane4 = 3;
    for (UINT32 i = 0; i < NUM_PATTERNS; i++)
    {
        UINT64 dataA = preLinkAData[0][i] |
                       preLinkAData[1][i] << 10 |
                       preLinkAData[2][i] << 20 |
                       (preLinkAData[3][i] & 0x3) << 30 |
                       static_cast<UINT64>(preLinkAData[lane4][i] & 0x3FC) << 30;

        UINT64 dataB = preLinkBData[0][i] |
                       preLinkBData[1][i] << 10 |
                       preLinkBData[2][i] << 20 |
                       (preLinkBData[3][i] & 0x3) << 30 |
                       static_cast<UINT64>(preLinkBData[lane4][i] & 0x3FC) << 30;

        if (!swapLink)
        {
            EncodedDataA[i] = dataA;
            EncodedDataB[i] = dataB;
        }
        else
        {
            EncodedDataA[i] = dataB;
            EncodedDataB[i] = dataA;
        }
    }

    if (m_SorLoopbackTestArgs.printModifiedCRC)
    {
        Printf(Tee::PriNormal, "\nModified Encoded TMDS patterns\n");
        Printf(Tee::PriNormal, "==================================\n");
        for (UINT32 i = 0; i < NUM_PATTERNS; i++)
        {
            Printf(Tee::PriNormal, "Entry %d\n", i);
            Printf(Tee::PriNormal, " EncodedDataA = %#016llx\n", EncodedDataA[i]);
            Printf(Tee::PriNormal, " EncodedDataB = %#016llx\n", EncodedDataB[i]);
            Printf(Tee::PriNormal, "\n");
        }
        Printf(Tee::PriNormal, "==================================\n");
    }

    UpdateCRC();

    return rc;
}

void LegacyLoopbackTester::UpdateCRC()
{
    UINT32 crcH = 0, crcM = 0, crcL = 0;
    UINT32 crcOut0 = 0, crcOut1 = 0;

    const UINT32 numPatterns = 6;
    for (UINT32 i = 0; i < numPatterns; i++)
    {
        GenerateCRC(LwU64_LO32(EncodedDataA[i]), &crcOut0, &crcOut1);
        crcL = crcOut0;
        crcM = crcOut1;

        GenerateCRC(LwU64_HI32(EncodedDataA[i]), &crcOut0, &crcOut1);
        crcH = crcOut0;

        CrcsA[i] = (static_cast<UINT64>(crcH) << 32)|(crcM << 16) |crcL;

        GenerateCRC(LwU64_LO32(EncodedDataB[i]), &crcOut0, &crcOut1);
        crcL = crcOut0;
        crcM = crcOut1;

        GenerateCRC(LwU64_HI32(EncodedDataB[i]), &crcOut0, &crcOut1);
        crcH = crcOut0;

        CrcsB[i] = (static_cast<UINT64>(crcH) << 32)|(crcM << 16) |crcL;
    }

    if (m_SorLoopbackTestArgs.printModifiedCRC)
    {
        Printf(Tee::PriNormal, "CRC modified values:\n");
        Printf(Tee::PriNormal, "========================================\n");
        for (UINT32 i = 0; i < numPatterns; i++)
        {
            Printf(Tee::PriNormal, "Entry %d\n",i);
            Printf(Tee::PriNormal, "CrcA:0x%016llx\n", CrcsA[i]);
            Printf(Tee::PriNormal, "CrcB:0x%016llx\n", CrcsB[i]);
        }
        Printf(Tee::PriNormal, "========================================\n");
    }
}

RC LegacyLoopbackTester::WritePatternAndAwaitEData
(
    UINT32 SorNumber,
    UINT32 PatternIdx,
    bool isProtocolTMDS,
    bool isLinkATestNeeded,
    bool isLinkBTestNeeded,
    UPHY_LANES lane
)
{
    RC rc;

    const UINT64* debugData  = isProtocolTMDS ? DebugData : DebugDataLVDS;
    const UINT64* encodedDataA  = isProtocolTMDS ? EncodedDataA : EncodedDataLVDS;
    const UINT64* encodedDataB  = isProtocolTMDS ? EncodedDataB : EncodedDataLVDS;
    RegHal &regs = m_pSubdev->Regs();

    VerbosePrintf("\nWriting Pattern %u: 0x%llx\n",
        PatternIdx, debugData[PatternIdx]);

    RegPairWr64(regs.LookupAddress(MODS_PDISP_SOR_DEBUGA0, SorNumber), debugData[PatternIdx]);
    RegPairWr64(regs.LookupAddress(MODS_PDISP_SOR_DEBUGB0, SorNumber), debugData[PatternIdx]);

    if (m_SorLoopbackTestArgs.waitForEDATA && isLinkATestNeeded)
    {
        VerbosePrintf("\nPolling for Expected Pattern: 0x%llx\n",
            encodedDataA[PatternIdx]);

        // Allow test to proceed even for failure - so as to analyze the complete flow
        PollPairRegValue(regs.LookupAddress(MODS_PDISP_SOR_EDATAA0, SorNumber),
            encodedDataA[PatternIdx], 40, m_SorLoopbackTestArgs.timeoutMs);
    }

    if (m_SorLoopbackTestArgs.waitForEDATA && isLinkBTestNeeded)
    {
        VerbosePrintf("\nPolling for Expected Pattern: 0x%llx\n",
            encodedDataB[PatternIdx]);

        // Allow test to proceed even for failure - so as to analyze the complete flow
        PollPairRegValue(regs.LookupAddress(MODS_PDISP_SOR_EDATAB0, SorNumber),
            encodedDataB[PatternIdx], 40, m_SorLoopbackTestArgs.timeoutMs);
    }

    return rc;
}

RC LegacyLoopbackTester::MatchCrcs
(
    UINT32 sorNumber,
    UINT32 patternIdx,
    bool isProtocolTMDS,
    bool isLinkATestNeeded,
    bool isLinkBTestNeeded,
    UPHY_LANES lane
)
{
    const UINT64* crcsA = isProtocolTMDS ? CrcsA : CrcsLVDS;
    const UINT64* crcsB = isProtocolTMDS ? CrcsB : CrcsLVDS;

    for (UINT32 retryIdx = 0; retryIdx <= m_SorLoopbackTestArgs.numRetries; retryIdx++)
    {
        bool linkAFailed = false;
        bool linkBFailed = false;

        RegHal &regs = m_pSubdev->Regs();
        UINT64 readCrcsA = RegPairRd64(regs.LookupAddress(MODS_PDISP_SOR_CCRCA0, sorNumber));
        UINT64 readCrcsB = RegPairRd64(regs.LookupAddress(MODS_PDISP_SOR_CCRCB0, sorNumber));

        if (isLinkATestNeeded && readCrcsA != crcsA[patternIdx])
        {
            linkAFailed = true;
        }
        if (isLinkBTestNeeded && readCrcsB != crcsB[patternIdx])
        {
            linkBFailed = true;
        }

        if (!(linkAFailed || linkBFailed))
        {
            break; // CRCs matched
        }
        else
        {
            Tee::Priority Pri = m_SorLoopbackTestArgs.printErrorDetails ?
                Tee::PriNormal : GetVerbosePrintPri();

            if (m_SorLoopbackTestArgs.printErrorDetails)
            {
                Printf(Tee::PriNormal, "   Failed pattern %d attempt %d:\n",
                    patternIdx, retryIdx + 1);
            }

            Printf(Pri, "Expected LinkA CRC: 0x%016llx\n", crcsA[patternIdx]);
            Printf(Pri, "    Read LinkA CRC: 0x%016llx\n", readCrcsA);

            Printf(Pri, "Expected LinkB CRC: 0x%016llx\n", crcsB[patternIdx]);
            Printf(Pri, "    Read LinkB CRC: 0x%016llx\n", readCrcsB);

            DumpSorInfoOnCrcMismatch(sorNumber);

            if (retryIdx >= m_SorLoopbackTestArgs.numRetries)
            {
                VerbosePrintf("Error: Actual crc does not match with Expected crc!!\n");

                if (!m_SorLoopbackTestArgs.ignoreCrcMiscompares)
                    return RC::SOR_CRC_MISCOMPARE;
            }
        }
    }

    return OK;
}

//------------------------------------------------------------------------------
// UPhyLoopbackTester
//------------------------------------------------------------------------------
UPhyLoopbackTester::UPhyLoopbackTester
(
    GpuSubdevice *pSubdev,
    Display *display,
    const SorLoopbackTestArgs& sorLoopbackTestArgs,
    Tee::Priority printPriority
)
:LoopbackTester(pSubdev, display, sorLoopbackTestArgs, printPriority)
{

}

RC UPhyLoopbackTester::SaveRegisters(UINT32 sorNumber)
{
    RC rc;
    RegHal &regs = m_pSubdev->Regs();
    CHECK_RC(LoopbackTester::SaveRegisters(sorNumber));

    m_SavedLinkControl = regs.Read32(MODS_PDISP_SOR_UPHY_LINK_CTRL0, sorNumber);
    m_SavedRxAligner = regs.Read32(MODS_PDISP_SOR_UPHY_RX_ALIGNER, sorNumber);
    m_SavedDriveLwrrent = RegPairRd64(
        regs.LookupAddress(MODS_PDISP_SOR_LANE_DRIVE_LWRRENT0, sorNumber));

    return rc;
}

RC UPhyLoopbackTester::RestoreRegisters(UINT32 sorNumber)
{
    RC rc;
    RegHal &regs = m_pSubdev->Regs();
    CHECK_RC(LoopbackTester::RestoreRegisters(sorNumber));

    regs.Write32(MODS_PDISP_SOR_UPHY_LINK_CTRL0, sorNumber, m_SavedLinkControl);
    regs.Write32(MODS_PDISP_SOR_UPHY_RX_ALIGNER, sorNumber, m_SavedRxAligner);
    RegPairWr64(
        regs.LookupAddress(MODS_PDISP_SOR_LANE_DRIVE_LWRRENT0, sorNumber),
        m_SavedDriveLwrrent);
    return rc;
}

RC UPhyLoopbackTester::EnableSORDebug(UINT32 sorNumber, UPHY_LANES lane)
{
    RC rc;

    CHECK_RC(PreEnableSORDebug(sorNumber, lane));

    CHECK_RC(LoopbackTester::EnableSORDebug(sorNumber, lane));

    CHECK_RC(PostEnableSORDebug(sorNumber));

    return rc;
}

RC UPhyLoopbackTester::PreEnableSORDebug(const UINT32 sorNumber, const UPHY_LANES lane)
{
    RC rc;

    RegHal &regs = m_pSubdev->Regs();
    const UINT32 sorLaneDriveLwrrent = 0x39;
    const UINT32 sorPattern = 0x3E1;
    UINT32 regValue = 0;

    switch (lane)
    {
        case UPHY_LANES::ZeroOne:
            VerbosePrintf("%s: UPHY - Lanes = 01, Setting Loop-off1/2 = NO, Loop-off0/3=Yes\n",
                                __FUNCTION__);
            regValue = regs.Read32(MODS_PDISP_SOR_UPHY_LINK_CTRL0, sorNumber);                
            regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_LINK_CTRL0_LOOP_OFF0_YES);
            regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_LINK_CTRL0_LOOP_OFF1_NO);
            regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_LINK_CTRL0_LOOP_OFF2_NO);
            regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_LINK_CTRL0_LOOP_OFF3_YES);
            regs.Write32(MODS_PDISP_SOR_UPHY_LINK_CTRL0, sorNumber, regValue);
            break;
        case UPHY_LANES::TwoThree:
            VerbosePrintf("%s: UPHY - Lanes = 23, Setting Loop-off1/2 = YES, Loop-off0/3=No\n",
                                __FUNCTION__);
            regValue = regs.Read32(MODS_PDISP_SOR_UPHY_LINK_CTRL0, sorNumber);
            regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_LINK_CTRL0_LOOP_OFF0_NO);
            regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_LINK_CTRL0_LOOP_OFF1_YES);
            regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_LINK_CTRL0_LOOP_OFF2_YES);
            regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_LINK_CTRL0_LOOP_OFF3_NO);
            regs.Write32(MODS_PDISP_SOR_UPHY_LINK_CTRL0, sorNumber, regValue);
            break;
        default:
            Printf(Tee::PriError, "%s: Unknown lane %u\n", __FUNCTION__,
                    static_cast<UINT32>(lane));
            return RC::SOFTWARE_ERROR;
    }

    VerbosePrintf("%s: UPHY - Setting SOF_INIT value of Aligner to  0x%08x\n",
                        __FUNCTION__, sorPattern);

    regValue = regs.Read32(MODS_PDISP_SOR_UPHY_RX_ALIGNER, sorNumber);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_RX_ALIGNER_SOF, sorPattern);
    regs.Write32(MODS_PDISP_SOR_UPHY_RX_ALIGNER, sorNumber, regValue);

    VerbosePrintf("%s: UPHY - Setting SOR_LANE_DRIVE_LWRRENT0 with 0x%08x on all lanes\n",
                        __FUNCTION__, sorLaneDriveLwrrent);
    regValue = regs.Read32(MODS_PDISP_SOR_LANE_DRIVE_LWRRENT0, sorNumber);
    regs.SetField(&regValue, MODS_PDISP_SOR_LANE_DRIVE_LWRRENT0_LANE2_DP_LANE0, sorLaneDriveLwrrent);
    regs.SetField(&regValue, MODS_PDISP_SOR_LANE_DRIVE_LWRRENT0_LANE1_DP_LANE1, sorLaneDriveLwrrent);
    regs.SetField(&regValue, MODS_PDISP_SOR_LANE_DRIVE_LWRRENT0_LANE0_DP_LANE2, sorLaneDriveLwrrent);
    regs.SetField(&regValue, MODS_PDISP_SOR_LANE_DRIVE_LWRRENT0_LANE3_DP_LANE3, sorLaneDriveLwrrent);
    regs.Write32(MODS_PDISP_SOR_LANE_DRIVE_LWRRENT0, sorNumber, regValue);

    VerbosePrintf("%s: UPHY - Setting SOR_LANE_DRIVE_LWRRENT1 with 0x%08x on all lanes\n",
                        __FUNCTION__, sorLaneDriveLwrrent);
    regValue = regs.Read32(MODS_PDISP_SOR_LANE_DRIVE_LWRRENT1, sorNumber);
    regs.SetField(&regValue, MODS_PDISP_SOR_LANE_DRIVE_LWRRENT1_LANE2_DP_LANE0, sorLaneDriveLwrrent);
    regs.SetField(&regValue, MODS_PDISP_SOR_LANE_DRIVE_LWRRENT1_LANE1_DP_LANE1, sorLaneDriveLwrrent);
    regs.SetField(&regValue, MODS_PDISP_SOR_LANE_DRIVE_LWRRENT1_LANE0_DP_LANE2, sorLaneDriveLwrrent);
    regs.SetField(&regValue, MODS_PDISP_SOR_LANE_DRIVE_LWRRENT1_LANE3_DP_LANE3, sorLaneDriveLwrrent);
    regs.Write32(MODS_PDISP_SOR_LANE_DRIVE_LWRRENT1, sorNumber, regValue);

    // Power on UPHY Lanes in case of UPHY test
    CHECK_RC(RxLanePowerUpHdmi(sorNumber));

    return rc;
}

RC UPhyLoopbackTester::RxLanePowerUpHdmi(const UINT32 sorNumber)
{
    VerbosePrintf("%s: UPHY - HDMI - RX Lane Power Up Sequence\n", __FUNCTION__);

    UINT32 regValue = 0;
    UINT32 rxRateId = 0;
    RegHal &regs = m_pSubdev->Regs();

    // Remove Reset
    regValue =  regs.Read32(MODS_PDISP_SOR_UPHY_LINK_CTRL0, sorNumber);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_LINK_CTRL0_RESET_NO);
    regs.Write32(MODS_PDISP_SOR_UPHY_LINK_CTRL0, sorNumber, regValue);

    // Read TX_RATE_ID, program the same for RX_RATE_ID
    regValue = regs.Read32(MODS_PDISP_SOR_UPHY_LINK_CTRL0, sorNumber);
    rxRateId = regs.GetField(regValue, MODS_PDISP_SOR_UPHY_LINK_CTRL0_TX_RATE_ID);
    VerbosePrintf("%s: UPHY RX Lane Rate ID = %d\n", __FUNCTION__, rxRateId);

    // Begin Power up Sequence - RX_CTRL0
    regValue = regs.Read32(MODS_PDISP_SOR_UPHY_RX_CTRL0, sorNumber);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_RX_CTRL0_CDR_EN_YES);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_RX_CTRL0_TERM_EN_YES);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_RX_CTRL0_RATE_PDIV, 0);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_RX_CTRL0_RATE_ID, rxRateId);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_RX_CTRL0_DATA_EN0, 0);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_RX_CTRL0_DATA_EN1, 0);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_RX_CTRL0_CAL_EN, 0);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_RX_CTRL0_EQ_RESET, 0);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_RX_CTRL0_EQ_TRAIN_EN, 0);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_RX_CTRL0_EOM_EN, 0);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_RX_CTRL0_CDR_RESET, 0);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_RX_CTRL0_BYP_MODE, 0);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_RX_CTRL0_BYP_EN, 0);
    regs.Write32(MODS_PDISP_SOR_UPHY_RX_CTRL0, sorNumber, regValue);

    // Set Lane Pwr
    regValue = regs.Read32(MODS_PDISP_SOR_UPHY_LANE_PWR, sorNumber);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_LANE_PWR_RX_IDDQ_OFF);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_LANE_PWR_RX_SLEEP0_YES);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_LANE_PWR_RX_SLEEP1_YES);
    regs.Write32(MODS_PDISP_SOR_UPHY_LANE_PWR, sorNumber, regValue);

    // Delay as per LWD_DP_over_USB_Functional_Description
    Platform::Delay(m_txCtrlSequenceDelayMs);

    // Bring Rx lanes out of sleep to prepare for calibrations
    VerbosePrintf("%s: UPHY - HDMI - RX Lane Out of Sleep\n", __FUNCTION__);
    regValue =  regs.Read32(MODS_PDISP_SOR_UPHY_LANE_PWR, sorNumber);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_LANE_PWR_RX_IDDQ_OFF);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_LANE_PWR_RX_SLEEP0_NO);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_LANE_PWR_RX_SLEEP1_NO);
    regs.Write32(MODS_PDISP_SOR_UPHY_LANE_PWR, sorNumber, regValue);

    // Calibration - set CAL_EN
    VerbosePrintf("%s: UPHY - HDMI - RX Calibration - Setting CAL_EN\n", __FUNCTION__);
    regValue =  regs.Read32(MODS_PDISP_SOR_UPHY_RX_CTRL0, sorNumber);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_RX_CTRL0_CAL_EN_YES);
    regs.Write32(MODS_PDISP_SOR_UPHY_RX_CTRL0, sorNumber, regValue);

    // Calibration - Poll for CAL_DONE = 1
    VerbosePrintf("%s: UPHY - HDMI - Polling for RX CAL_DONE = 1\n", __FUNCTION__);
    PollRegValue(regs.LookupAddress(MODS_PDISP_SOR_UPHY_RX_STATUS_LANE0, sorNumber),
                 regs.SetField(MODS_PDISP_SOR_UPHY_RX_STATUS_LANE0_CAL_DONE_YES),
                 regs.LookupMask(MODS_PDISP_SOR_UPHY_RX_STATUS_LANE0_CAL_DONE),
                 m_SorLoopbackTestArgs.timeoutMs);

    // Calibration - clear CAL_EN
    VerbosePrintf("%s: UPHY - HDMI - Clearing CAL_EN\n", __FUNCTION__);
    regValue =  regs.Read32(MODS_PDISP_SOR_UPHY_RX_CTRL0, sorNumber);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_RX_CTRL0_CAL_EN_NO);
    regs.Write32(MODS_PDISP_SOR_UPHY_RX_CTRL0, sorNumber, regValue);

    // Calibration - Poll for CAL_DONE = 0
    VerbosePrintf("%s: UPHY - HDMI - Polling for CAL_DONE = 0\n", __FUNCTION__);
    PollRegValue(regs.LookupAddress(MODS_PDISP_SOR_UPHY_RX_STATUS_LANE0, sorNumber),
                 regs.SetField(MODS_PDISP_SOR_UPHY_RX_STATUS_LANE0_CAL_DONE_NO),
                 regs.LookupMask(MODS_PDISP_SOR_UPHY_RX_STATUS_LANE0_CAL_DONE),
                 m_SorLoopbackTestArgs.timeoutMs);
    VerbosePrintf("%s: UPHY - HDMI - RX Calibration Completed\n", __FUNCTION__);

    // Enable RX Lanes
    VerbosePrintf("%s: UPHY - HDMI - Enable Rx Lanes\n", __FUNCTION__);
    regValue =  regs.Read32(MODS_PDISP_SOR_UPHY_RX_CTRL0, sorNumber);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_RX_CTRL0_DATA_EN0_YES);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_RX_CTRL0_DATA_EN1_YES);
    regs.Write32(MODS_PDISP_SOR_UPHY_RX_CTRL0, sorNumber, regValue);
    VerbosePrintf("%s: UPHY - HDMI - Rx Lanes Enabled\n", __FUNCTION__);

    return OK;
}

RC UPhyLoopbackTester::PostEnableSORDebug(UINT32 sorNumber)
{
    // 36 bit pattern
    const UINT64 cdrPattern = 0x555555555ULL;
    RegHal &regs = m_pSubdev->Regs();
    RegPairWr64(regs.LookupAddress(MODS_PDISP_SOR_DEBUGA0, sorNumber), cdrPattern);

    const UINT32 SLEEP_VAL_FROM_ARCH_TEST = 5;
    const UINT32 ADDITIONAL_SLEEP_MULTIPLIER = 2;
    const UINT32 FINAL_SLEEP_TIME = SLEEP_VAL_FROM_ARCH_TEST * ADDITIONAL_SLEEP_MULTIPLIER;

    Platform::SleepUS(FINAL_SLEEP_TIME);

    return OK;
}

RC UPhyLoopbackTester::DisableSORDebug(UINT32 sorNumber)
{
    StickyRC rc;

    rc = LoopbackTester::DisableSORDebug(sorNumber);
    rc = RxLanePowerDownHdmi(sorNumber);

    return rc;
}

RC UPhyLoopbackTester::RxLanePowerDownHdmi(const UINT32 sorNumber)
{
    VerbosePrintf("%s: UPHY - HDMI - RX Lane Power Down Sequence\n", __FUNCTION__);
    RegHal &regs = m_pSubdev->Regs();
    UINT32 regValue = 0;

    // Enable RX Lanes
    VerbosePrintf("%s: UPHY - HDMI - Disable Rx Lanes\n", __FUNCTION__);
    regValue = regs.Read32(MODS_PDISP_SOR_UPHY_RX_CTRL0, sorNumber);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_RX_CTRL0_DATA_EN0_NO);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_RX_CTRL0_DATA_EN1_NO);
    regs.Write32(MODS_PDISP_SOR_UPHY_RX_CTRL0, sorNumber, regValue);
    VerbosePrintf("%s: UPHY - HDMI - Rx Lanes Disabled\n", __FUNCTION__);

    // Delay as per LWD_DP_over_USB_Functional_Description
    Platform::Delay(m_txCtrlSequenceDelayMs);

    // Bring Rx lanes to sleepout of sleep to prepare for calibrations
    VerbosePrintf("%s: UPHY - HDMI - Setting RX IDDQ = No, Sleep = No\n", __FUNCTION__);
    regValue = regs.Read32(MODS_PDISP_SOR_UPHY_LANE_PWR, sorNumber);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_LANE_PWR_RX_IDDQ_OFF);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_LANE_PWR_RX_SLEEP0_NO);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_LANE_PWR_RX_SLEEP1_NO);
    regs.Write32(MODS_PDISP_SOR_UPHY_LANE_PWR, sorNumber, regValue);

    // Delay as per LWD_DP_over_USB_Functional_Description
    Platform::Delay(m_txCtrlSequenceDelayMs);

    VerbosePrintf("%s: UPHY - HDMI - Setting RX IDDQ = No, Sleep = Yes\n", __FUNCTION__);
    regValue = regs.Read32(MODS_PDISP_SOR_UPHY_LANE_PWR, sorNumber);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_LANE_PWR_RX_IDDQ_OFF);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_LANE_PWR_RX_SLEEP0_YES);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_LANE_PWR_RX_SLEEP1_YES);
    regs.Write32(MODS_PDISP_SOR_UPHY_LANE_PWR, sorNumber, regValue);

    // Begin Power up Sequence - RX_CTRL0
    VerbosePrintf("%s: UPHY - HDMI - Setting RX CDR_EN = No, TERM_EN = No\n", __FUNCTION__);
    regValue = regs.Read32(MODS_PDISP_SOR_UPHY_RX_CTRL0, sorNumber);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_RX_CTRL0_CDR_EN_NO);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_RX_CTRL0_TERM_EN_NO);
    regs.Write32(MODS_PDISP_SOR_UPHY_RX_CTRL0, sorNumber, regValue);
    
    // Delay as per LWD_DP_over_USB_Functional_Description
    Platform::Delay(m_txCtrlSequenceDelayMs);

    VerbosePrintf("%s: UPHY - HDMI - Setting RX IDDQ = Yes, Sleep = Yes\n", __FUNCTION__);
    regValue = regs.Read32(MODS_PDISP_SOR_UPHY_LANE_PWR, sorNumber);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_LANE_PWR_RX_IDDQ_ON);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_LANE_PWR_RX_SLEEP0_YES);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_LANE_PWR_RX_SLEEP1_YES);
    regs.Write32(MODS_PDISP_SOR_UPHY_LANE_PWR, sorNumber, regValue);
    VerbosePrintf("%s: UPHY - HDMI - RX Lane power down complete\n", __FUNCTION__);

    return OK;
}

RC UPhyLoopbackTester::RecalcEncodedDataAndCrcs(UINT32 sorNumber)
{
    // Uphy only has 2 RX lanes, so at any time we will loop TX0/1 to RX0/11 or TX2/3 to RX0/1
    // Expected data is always 20 bits. It could be the upper 20 or lower 20 depending on mode
    // Similarly, CRC needs to be regenerated for only 15:0 and 31:16 again for UPHY cases
    for (UINT32 i = 0; i < NUM_PATTERNS; i++)
    {
        // Based on _LOOP_OFF2/3 = YES, the data is TX2(9:0) | TX1(9:0) 
        EncodedDataUPhy01[i] = (DebugData[i] >> 10) & ((1<<20)-1);

        // Based on _LOOP_OFF0/1 = YES, the data is TX3(9:0) | TX0(9:0)
        EncodedDataUPhy23[i] = ((DebugData[i] >> 30) << 10) | (DebugData[i] & ((1<<10) -1));
    }

    if (m_SorLoopbackTestArgs.printModifiedCRC)
    {
        Printf(Tee::PriNormal, "\nModified Encoded TMDS patterns for UPhy\n");
        Printf(Tee::PriNormal, "==================================\n");
        for (UINT32 i = 0; i < NUM_PATTERNS; i++)
        {
            Printf(Tee::PriNormal, "Entry %d\n", i);
            Printf(Tee::PriNormal, " EncodedDataUPhy01 = %#016llx\n", EncodedDataUPhy01[i]);
            Printf(Tee::PriNormal, " EncodedDataUPhy23 = %#016llx\n", EncodedDataUPhy23[i]);
            Printf(Tee::PriNormal, "\n");
        }
        Printf(Tee::PriNormal, "==================================\n");
    }

    UpdateCRC();

    return OK;
}

void UPhyLoopbackTester::UpdateCRC()
{
    // CRC needs to be regenerated for only 15:0 and 31:16 again for UPHY cases
    UINT32 crcM = 0, crcL = 0;
    UINT32 crcOut0 = 0, crcOut1 = 0;

    const UINT32 numPatterns = 6;
    for (UINT32 i = 0; i < numPatterns; i++)
    {
        GenerateCRC(LwU64_LO32(EncodedDataUPhy01[i]), &crcOut0, &crcOut1);
        crcL = crcOut0;
        crcM = crcOut1;

        CrcsUPhy01[i] = (crcM << 16) |crcL;

        GenerateCRC(LwU64_LO32(EncodedDataUPhy23[i]), &crcOut0, &crcOut1);
        crcL = crcOut0;
        crcM = crcOut1;

        CrcsUPhy23[i] = (crcM << 16) |crcL;
    }

    if (m_SorLoopbackTestArgs.printModifiedCRC)
    {
        Printf(Tee::PriNormal, "CRC modified values for UPhy:\n");
        Printf(Tee::PriNormal, "========================================\n");
        for (UINT32 i = 0; i < numPatterns; i++)
        {
            Printf(Tee::PriNormal, "Entry %d\n", i);
            Printf(Tee::PriNormal, "CrcsUPhy01:0x%016llx\n", CrcsUPhy01[i]);
            Printf(Tee::PriNormal, "CrcsUPhy23:0x%016llx\n", CrcsUPhy23[i]);
        }
        Printf(Tee::PriNormal, "========================================\n");
    }
}

RC UPhyLoopbackTester::WritePatternAndAwaitEData
(
    UINT32 SorNumber,
    UINT32 PatternIdx,
    bool isProtocolTMDS,
    bool isLinkATestNeeded,
    bool isLinkBTestNeeded,
    UPHY_LANES lane
)
{
    RC rc;
    RegHal &regs = m_pSubdev->Regs();
    CHECK_RC(PreWritePatternSetup(SorNumber));

    const UINT64* encodedData = (lane == UPHY_LANES::ZeroOne) ? EncodedDataUPhy01 : EncodedDataUPhy23;
    const UINT64* debugData  = DebugData;

    VerbosePrintf("\nWriting Pattern %u: 0x%llx\n", PatternIdx, debugData[PatternIdx]);

    RegPairWr64(regs.LookupAddress(MODS_PDISP_SOR_DEBUGA0, SorNumber), debugData[PatternIdx]);
    RegPairWr64(regs.LookupAddress(MODS_PDISP_SOR_DEBUGB0, SorNumber), debugData[PatternIdx]);

    if (m_SorLoopbackTestArgs.waitForEDATA && isLinkATestNeeded)
    {
        VerbosePrintf("\nPolling for Expected Pattern: 0x%llx\n", encodedData[PatternIdx]);

        CHECK_RC(PollPairRegValue(regs.LookupAddress(MODS_PDISP_SOR_EDATAA0, SorNumber),
            encodedData[PatternIdx], 20, m_SorLoopbackTestArgs.timeoutMs));
    }

    if (m_SorLoopbackTestArgs.waitForEDATA && isLinkBTestNeeded)
    {
        VerbosePrintf("\nPolling for Expected Pattern: 0x%llx\n", encodedData[PatternIdx]);

        CHECK_RC(PollPairRegValue(regs.LookupAddress(MODS_PDISP_SOR_EDATAB0, SorNumber),
            encodedData[PatternIdx], 20, m_SorLoopbackTestArgs.timeoutMs));
    }

    return OK;

}

RC UPhyLoopbackTester::PreWritePatternSetup(UINT32 sorNumber)
{
    const UINT64 sorPattern = (0x3E1ULL << 30) | (0x3E1ULL << 20) | (0x3E1ULL << 10) | 0x3E1ULL;
    const UINT32 sof = 0x3E1;
    UINT32 regValue = 0;
    const UINT32 timeOutMultMs = 10;

    RegHal &regs = m_pSubdev->Regs();
    VerbosePrintf("%s:UPHY - Set SOR test pattern 0x%llx\n", __FUNCTION__, sorPattern);
    RegPairWr64(regs.LookupAddress(MODS_PDISP_SOR_DEBUGA0, sorNumber), sorPattern);
    RegPairWr64(regs.LookupAddress(MODS_PDISP_SOR_DEBUGB0, sorNumber), sorPattern);

    VerbosePrintf("%s:UPHY - Enabling the aligner with SOF = 0x%08x\n", __FUNCTION__, sof);
    regValue = regs.Read32(MODS_PDISP_SOR_UPHY_RX_ALIGNER, sorNumber);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_RX_ALIGNER_BYPASS_NO);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_RX_ALIGNER_REALIGN_NO);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_RX_ALIGNER_SOF, sof);
    regs.Write32(MODS_PDISP_SOR_UPHY_RX_ALIGNER, sorNumber, regValue);

    regValue = regs.Read32(MODS_PDISP_SOR_UPHY_RX_ALIGNER, sorNumber);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_RX_ALIGNER_REALIGN_YES);
    regs.Write32(MODS_PDISP_SOR_UPHY_RX_ALIGNER, sorNumber, regValue);

    VerbosePrintf("%s:UPHY - Poll for Aligner Synchronization - Lane 0\n", __FUNCTION__);
    // Allow test to proceed even for failure - so as to analyze the complete flow
    PollRegValue(regs.LookupAddress(MODS_PDISP_SOR_UPHY_RX_ALIGNER, sorNumber),
                 regs.SetField(MODS_PDISP_SOR_UPHY_RX_ALIGNER_LANE0_SYNCED_YES),
                 regs.LookupMask(MODS_PDISP_SOR_UPHY_RX_ALIGNER_LANE0_SYNCED),
                 m_SorLoopbackTestArgs.timeoutMs*timeOutMultMs);

    VerbosePrintf("%s:UPHY - Poll for Aligner Synchronization - Lane 1\n", __FUNCTION__);
    // Allow test to proceed even for failure - so as to analyze the complete flow
    PollRegValue(regs.LookupAddress(MODS_PDISP_SOR_UPHY_RX_ALIGNER, sorNumber),
                 regs.SetField(MODS_PDISP_SOR_UPHY_RX_ALIGNER_LANE1_SYNCED_YES),
                 regs.LookupMask(MODS_PDISP_SOR_UPHY_RX_ALIGNER_LANE1_SYNCED),
                 m_SorLoopbackTestArgs.timeoutMs*timeOutMultMs);

    regValue = regs.Read32(MODS_PDISP_SOR_UPHY_RX_ALIGNER, sorNumber);
    regs.SetField(&regValue, MODS_PDISP_SOR_UPHY_RX_ALIGNER_REALIGN_NO);
    regs.Write32(MODS_PDISP_SOR_UPHY_RX_ALIGNER, sorNumber, regValue);

    return OK;
}

RC UPhyLoopbackTester::MatchCrcs
(
    UINT32 sorNumber,
    UINT32 patternIdx,
    bool isProtocolTMDS,
    bool isLinkATestNeeded,
    bool isLinkBTestNeeded,
    UPHY_LANES lane
)
{
    const UINT64* crcs = lane == UPHY_LANES::ZeroOne ? CrcsUPhy01 : CrcsUPhy23;
    RegHal &regs = m_pSubdev->Regs();

    for (UINT32 retryIdx = 0; retryIdx <= m_SorLoopbackTestArgs.numRetries; retryIdx++)
    {
        bool linkAFailed = false;
        bool linkBFailed = false;
        UINT64 readCrcsA = RegPairRd64(regs.LookupAddress(MODS_PDISP_SOR_CCRCA0, sorNumber));
        UINT64 readCrcsB = RegPairRd64(regs.LookupAddress(MODS_PDISP_SOR_CCRCB0, sorNumber));

        if (isLinkATestNeeded && readCrcsA != crcs[patternIdx])
        {
            linkAFailed = true;
        }
        if (isLinkBTestNeeded && readCrcsB != crcs[patternIdx])
        {
            linkBFailed = true;
        }

        if (!(linkAFailed || linkBFailed))
        {
            break; // CRCs matched
        }
        else
        {

            Tee::Priority Pri = m_SorLoopbackTestArgs.printErrorDetails ?
                        Tee::PriNormal : GetVerbosePrintPri();

            if (m_SorLoopbackTestArgs.printErrorDetails)
            {
                Printf(Tee::PriNormal, "   Failed pattern %d attempt %d:\n",
                    patternIdx, retryIdx + 1);
            }

            Printf(Pri, "Expected LinkA CRC: 0x%016llx\n", crcs[patternIdx]);
            Printf(Pri, "    Read LinkA CRC: 0x%016llx\n", readCrcsA);

            Printf(Pri, "Expected LinkB CRC: 0x%016llx\n", crcs[patternIdx]);
            Printf(Pri, "    Read LinkB CRC: 0x%016llx\n", readCrcsB);

            DumpSorInfoOnCrcMismatch(sorNumber);

            if (retryIdx >= m_SorLoopbackTestArgs.numRetries)
            {
                VerbosePrintf("Error: Actual crc does not match with Expected crc!!\n");
                if (!m_SorLoopbackTestArgs.ignoreCrcMiscompares)
                    return RC::SOR_CRC_MISCOMPARE;
            }
        }
    }
    return OK;
}

//------------------------------------------------------------------------------
// IobistLoopbackTester
//------------------------------------------------------------------------------
IobistLoopbackTester::IobistLoopbackTester
(
    GpuSubdevice *pSubdev,
    Display *pDisplay,
    const SorLoopbackTestArgs& sorLoopbackTestArgs,
    Tee::Priority printPriority
)
:LoopbackTester(pSubdev, pDisplay, sorLoopbackTestArgs, printPriority)
{
    m_pRegHal = &m_pSubdev->Regs();
}

RC IobistLoopbackTester::RunLoopbackTest
(
    const DisplayID &displayID,
    UINT32 sorNumber,
    UINT32 protocol,
    Display::Encoder::Links link,
    UPHY_LANES lane
)
{
    RC rc;

    // Check SOR bypass mode
    if (!m_FrlMode
         && m_LwDispRegHal->Test32(MODS_PDISP_FE_CMGR_CLK_SOR_MODE_BYPASS_DP_NORMAL, sorNumber))
    {
        Printf(Tee::PriError, "SOR %u already connected to DP\n", sorNumber);
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    CHECK_RC(EnableSORDebug(sorNumber));

    bool isProtocolTMDS = false;
    bool isLinkATestNeeded = false;
    bool isLinkBTestNeeded = false;

    CHECK_RC(GetLinksUnderTestInfo(sorNumber, protocol, &isProtocolTMDS,
                &isLinkATestNeeded, &isLinkBTestNeeded));

    INT32 priPadlinkNum = isLinkATestNeeded ? (link.Primary - 'A') : ILWALID_PADLINK_NUM;
    INT32 secPadlinkNum = isLinkBTestNeeded ? (link.Secondary - 'A') : ILWALID_PADLINK_NUM;

    if (priPadlinkNum == ILWALID_PADLINK_NUM && secPadlinkNum == ILWALID_PADLINK_NUM)
    {
        VerbosePrintf("Test not needed on both primary and secondary link "
                "for sor %u, displayID 0x%x, protocol = %u\n",
                sorNumber, static_cast<UINT32>(displayID), protocol);
        return rc;
    }

    CHECK_RC(RunIobistSequence(sorNumber, 0, priPadlinkNum, secPadlinkNum));
    return rc;
}

RC IobistLoopbackTester::EnableSORDebug(UINT32 sorNumber)
{
    if (m_pRegHal->IsSupported(MODS_PDISP_SOR_TEST_DEEP_LOOPBK_ENABLE))
    {
        m_LwDispRegHal->Write32(MODS_PDISP_SOR_TEST_DEEP_LOOPBK_ENABLE, sorNumber);
    }

    return RC::OK;
}

RC IobistLoopbackTester::RunIobistSequence
(
    UINT32 sorNumber,
    UINT32 patternIdx,
    INT32 priPadlinkNum,
    INT32 secPadlinkNum
)
{
    // Reference: //hw/doc/mmplex/display/4.0/specifications/LWDisplay_XBAR_AFIFO_IAS.docx
    // Section 5.3

    RC rc;
    {
        CHECK_RC(ValidateIobistLoopCount());
        DEFER
        {
            rc = CleanupRegisters(sorNumber);
        };

        Printf(Tee::PriDebug, "Starting IOBIST Sequence\n");

        CHECK_RC(EnableIobistClock(sorNumber));
        CHECK_RC(EnableIobistOutput(sorNumber));
        CHECK_RC(ConfigureIobist(sorNumber));
        CHECK_RC(RunSequenceOnEachSublink(sorNumber, priPadlinkNum, secPadlinkNum));
    }
    return rc;
}

RC IobistLoopbackTester::ValidateIobistLoopCount()
{
    // Eventually we are supposed to write the value of iobistLoopCount to
    // MODS_DISP_DUALIO_BIST_ERRORCOUNT_DPV_LOOPCNT field whose size is 5 bits
    // IBOSIT would run (2 ^ iobistLoopCount) number of loops
    constexpr UINT32 maxIobistLoopCount = 0x1F;

    // We are writing a value of 128 in MODS_DISP_DUALIO_BIST_CAPTURESTART_DPV_CAPTURESTART
    // This value signifies the number of loops to be ignored before IOBIST starts capturing
    // CRCs, needed for IOBIST to stabalize.
    // The number of loops run should be greater than 128 (2 ^ 8 = 256).
    constexpr UINT32 minIobistLoopCount = 0x8;

    if (m_SorLoopbackTestArgs.iobistLoopCount > maxIobistLoopCount)
    {
        Printf(Tee::PriWarn, "IobistLoopCount (%u) exceeds the max allowed value (%u). "
               "Using max value instead\n",
               m_SorLoopbackTestArgs.iobistLoopCount, maxIobistLoopCount);
        m_SorLoopbackTestArgs.iobistLoopCount = maxIobistLoopCount;
    }
    if (m_SorLoopbackTestArgs.iobistLoopCount
        && (m_SorLoopbackTestArgs.iobistLoopCount < minIobistLoopCount))
    {
        Printf(Tee::PriWarn, "IobistLoopCount (%u) is lower the min allowed value (%u). "
               "Using min allowed value instead\n",
               m_SorLoopbackTestArgs.iobistLoopCount, minIobistLoopCount);
        m_SorLoopbackTestArgs.iobistLoopCount = minIobistLoopCount;
    }
    return RC::OK;
}

RC IobistLoopbackTester::EnableIobistClock(UINT32 sorNumber)
{
    RC rc;

    UINT32 regData = 0;
    CHECK_RC(ReadIobistReg(MODS_DISP_DUALIO_BIST_CONFIG, sorNumber, &regData));

    m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_CONFIG_CFGCLKGATEEN, 0x1);
    m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_CONFIG_DPV_LOOPSYMBOLCNTEN, 0x1);
    m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_CONFIG_CAPTUREBUFDIR_CONTROL, 0x1);

    CHECK_RC(WriteIobistReg(MODS_DISP_DUALIO_BIST_CONFIG, sorNumber, regData));

    return rc;
}

RC IobistLoopbackTester::EnableIobistOutput(UINT32 sorNumber)
{
    RC rc;

    UINT32 regData = 0;
    CHECK_RC(ReadIobistReg(MODS_DISP_DUALIO_BIST_CONFIGREG, sorNumber, &regData));

    m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_CONFIGREG_DPG_TXDATAMUXSEL_IN, 0x1);
    m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_CONFIGREG_RATE_OVERRIDE_IN, 0x1);

    const UINT32 mRate = m_FrlMode ? 0x3 : 0x1;
    const UINT32 nRate = m_FrlMode ? 0x5 : 0x2;
    
    m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_CONFIGREG_M_RATE_IN, mRate);
    m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_CONFIGREG_N_RATE_IN, nRate);

    CHECK_RC(WriteIobistReg(MODS_DISP_DUALIO_BIST_CONFIGREG, sorNumber, regData));

    return rc;
}

RC IobistLoopbackTester::ConfigureIobist(UINT32 sorNumber)
{
    RC rc;

    // Configure IOBIST to capture and stop on the 1st error
    const UINT32 stopOnError = m_SorLoopbackTestArgs.iobistLoopCount ? 0x0 : 0x1;

    UINT32 regData = 0;
    CHECK_RC(ReadIobistReg(MODS_DISP_DUALIO_BIST_ERRORCONFIG, sorNumber, &regData));

    m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_ERRORCONFIG_DPV_STOPONERROR, stopOnError);
    m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_ERRORCONFIG_DPV_STOPERRORCOUNT, 0x0);

    CHECK_RC(WriteIobistReg(MODS_DISP_DUALIO_BIST_ERRORCONFIG, sorNumber, regData));

    // set capture at N cycles after STARTTEST
    constexpr UINT32 captureAfterCycles = 128;

    regData = 0;
    CHECK_RC(ReadIobistReg(MODS_DISP_DUALIO_BIST_CAPTURESTART, sorNumber, &regData));

    m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_CAPTURESTART_DPV_CAPTURESTART, 
            captureAfterCycles);

    CHECK_RC(WriteIobistReg(MODS_DISP_DUALIO_BIST_CAPTURESTART, sorNumber, regData));
    
    // Config to run maximum 2^N cycles from STARTTEST

    constexpr UINT32 defaultIobistLoops = 16;
    const UINT32 loopCount = m_SorLoopbackTestArgs.iobistLoopCount
        ? m_SorLoopbackTestArgs.iobistLoopCount : defaultIobistLoops;

    regData = 0;
    CHECK_RC(ReadIobistReg(MODS_DISP_DUALIO_BIST_ERRORCOUNT, sorNumber, &regData));

    m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_ERRORCOUNT_DPV_LOOPCNT, loopCount);

    CHECK_RC(WriteIobistReg(MODS_DISP_DUALIO_BIST_ERRORCOUNT, sorNumber, regData));

    return rc;
}

RC IobistLoopbackTester::RunSequenceOnEachSublink
(
    UINT32 sorNumber,
    INT32 priPadlinkNum,
    INT32 secPadlinkNum
)
{
    RC rc;

    CHECK_RC(SelectSorSublink(sorNumber, priPadlinkNum, secPadlinkNum));

    if (priPadlinkNum != ILWALID_PADLINK_NUM)
    {
        // Connect the sorSublink to PAD
        constexpr UINT32 sorSublinkToConfig = 0;
        CHECK_RC(ConnectSorSublinkToPAD(sorNumber, priPadlinkNum, sorSublinkToConfig));
    }
    if (secPadlinkNum != ILWALID_PADLINK_NUM)
    {
        constexpr UINT32 sorSublinkToConfig = 1;
        CHECK_RC(ConnectSorSublinkToPAD(sorNumber, secPadlinkNum, sorSublinkToConfig));
    }

    // Allow capture
    CHECK_RC(AllowCapture(sorNumber));
    
    // Enable afifo and loopback
    CHECK_RC(EnableAfifoAndLoopback(sorNumber));

    const UINT32 sorSublink =
          ((m_SorLoopbackTestArgs.iobistCaptureBufferForSecLink && (secPadlinkNum != ILWALID_PADLINK_NUM))
            || (priPadlinkNum == ILWALID_PADLINK_NUM))
           ? 1 : 0;
    // loop and test 4 lanes for each sorSublink
    CHECK_RC(RunSequenceOnEachLaneForSublink(sorNumber, sorSublink, priPadlinkNum, secPadlinkNum));
    return rc;
}

RC IobistLoopbackTester::SelectSorSublink
(
    UINT32 sorNumber,
    INT32 priPadlinkNum,
    INT32 secPadlinkNum
)
{
    RC rc;

    const UINT32 sorSublink =
        ((m_SorLoopbackTestArgs.iobistCaptureBufferForSecLink && (secPadlinkNum != ILWALID_PADLINK_NUM))
         || (priPadlinkNum == ILWALID_PADLINK_NUM))
        ? 1 : 0;

    UINT32 regData = 0;
    CHECK_RC(ReadIobistReg(MODS_DISP_DUALIO_BIST_SELECTCONFIG, sorNumber, &regData));

    m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_SELECTCONFIG_DPV_SELECTLANE, sorSublink);

    CHECK_RC(WriteIobistReg(MODS_DISP_DUALIO_BIST_SELECTCONFIG, sorNumber, regData));

    return rc;
}

RC IobistLoopbackTester::ConnectSorSublinkToPAD
(
    UINT32 sorNumber,
    INT32 padlinkNum,
    UINT32 sorSublinkToConfig
)
{
    RC rc;

    CHECK_RC(ConfigureSubLink(sorNumber, padlinkNum, sorSublinkToConfig));
    CHECK_RC(SelectPRBSPattern(sorNumber, sorSublinkToConfig));

    return rc;
}

RC IobistLoopbackTester::AllowCapture(UINT32 sorNumber)
{
    RC rc;

    UINT32 regData = 0;
    CHECK_RC(ReadIobistReg(MODS_DISP_DUALIO_BIST_CONFIG, sorNumber, &regData));

    m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_CONFIG_DPV_CAPTUREEN, 1);

    CHECK_RC(WriteIobistReg(MODS_DISP_DUALIO_BIST_CONFIG, sorNumber, regData));

    return rc;
}

RC IobistLoopbackTester::EnableAfifoAndLoopback(UINT32 sorNumber)
{
    RC rc;

    UINT32 regData = 0;
    CHECK_RC(ReadIobistReg(MODS_DISP_DUALIO_BIST_CONFIGREG_2, sorNumber, &regData));

    m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_CONFIGREG_2_FIFO_WRITE_EN_PRESYNC_IN, 1);
    m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_CONFIGREG_2_FIFO_READ_EN_PRESYNC_IN, 1);

    CHECK_RC(WriteIobistReg(MODS_DISP_DUALIO_BIST_CONFIGREG_2, sorNumber, regData));

    return rc;
}

RC IobistLoopbackTester::RunSequenceOnEachLaneForSublink
(
    UINT32 sorNumber,
    UINT32 sorSublink,
    INT32 priPadlinkNum,
    INT32 secPadlinkNum
)
{
    RC rc;
    StickyRC stickyRc;

    {
        for (UINT32 lane = 0; lane < 4; ++lane)
        {
            // select specified lane
            CHECK_RC(SelectSpecifiedLane(sorNumber, lane));

            DEFER
            {
                // Stop capture
                stickyRc = StopCapture(sorNumber);
            };

            // Start test
            CHECK_RC(StartTest(sorNumber));

            // Wait for data flow back. Waiting for few hundred microsecond is good enough
            Tasker::Sleep(1);

            // Start DPV
            CHECK_RC(StartDPV(sorNumber));


            // Check test results
            CHECK_RC(CheckTestResults(sorNumber, lane, sorSublink, priPadlinkNum, secPadlinkNum));
        }
    }
    return stickyRc;
}

RC IobistLoopbackTester::SelectSpecifiedLane(UINT32 sorNumber, UINT32 lane)
{
    RC rc;

    UINT32 regData = 0;
    CHECK_RC(ReadIobistReg(MODS_DISP_DUALIO_BIST_CONFIGREG_2, sorNumber, &regData));

    m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_CONFIGREG_2_LANE_RX_MUX_SEL_PRESYNC_IN, 
            lane);

    CHECK_RC(WriteIobistReg(MODS_DISP_DUALIO_BIST_CONFIGREG_2, sorNumber, regData));

    return rc;
}

RC IobistLoopbackTester::StopCapture(UINT32 sorNumber)
{

    StickyRC stickyRc;

    UINT32 regData = 0;
    stickyRc = ReadIobistReg(MODS_DISP_DUALIO_BIST_CONFIG, sorNumber, &regData);

    m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_CONFIG_DPV_HOT_RESET, 0);
    m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_CONFIG_STARTTEST, 0);

    stickyRc = WriteIobistReg(MODS_DISP_DUALIO_BIST_CONFIG, sorNumber, regData);

    return stickyRc;
}

RC IobistLoopbackTester::StartTest(UINT32 sorNumber)
{
    RC rc;

    UINT32 regData = 0;
    CHECK_RC(ReadIobistReg(MODS_DISP_DUALIO_BIST_CONFIG, sorNumber, &regData));

    m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_CONFIG_STARTTEST, 1);

    CHECK_RC(WriteIobistReg(MODS_DISP_DUALIO_BIST_CONFIG, sorNumber, regData));

    return rc;
}

RC IobistLoopbackTester::StartDPV(UINT32 sorNumber)
{
    RC rc;

    UINT32 regData = 0;
    CHECK_RC(ReadIobistReg(MODS_DISP_DUALIO_BIST_CONFIG, sorNumber, &regData));

    m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_CONFIG_DPV_HOT_RESET, 1);

    CHECK_RC(WriteIobistReg(MODS_DISP_DUALIO_BIST_CONFIG, sorNumber, regData));

    return rc;
}

RC IobistLoopbackTester::CheckTestResults
(
    UINT32 sorNumber,
    UINT32 lane,
    UINT32 sorSublink,
    INT32 priPadlinkNum,
    INT32 secPadlinkNum
)
{
    RC rc;

    constexpr UINT32 verifyCompleteExpectedValue = 0x1;
    auto pollVerifyComplete = [&]()->bool
    {
        UINT32 verifyCompleteFieldValue;
        Printf(Tee::PriDebug, "-------------------------------------------------------\n");
        
        UINT32 regData = 0;
        CHECK_RC(ReadIobistReg(MODS_DISP_DUALIO_BIST_STATUS, sorNumber, &regData));

        verifyCompleteFieldValue = m_pRegHal->GetField(regData, 
                MODS_DISP_DUALIO_BIST_STATUS_DPV_VERIFYCOMPLETE);
        return verifyCompleteFieldValue == verifyCompleteExpectedValue;
    };

    // Wait for test to complete
    CHECK_RC(Tasker::PollHw(m_SorLoopbackTestArgs.timeoutMs,
                pollVerifyComplete, __FUNCTION__));

    UINT32 errStatusPri = 0x0;
    UINT32 errStatusSec = 0x0;
    if (priPadlinkNum != ILWALID_PADLINK_NUM)
    {
        CHECK_RC(ReadIobistReg(MODS_DISP_DUALIO_BIST_LANE_00_STATUS,
                               sorNumber,
                               &errStatusPri));
    }
    if (secPadlinkNum != ILWALID_PADLINK_NUM)
    {

        CHECK_RC(ReadIobistReg(MODS_DISP_DUALIO_BIST_LANE_01_STATUS,
                               sorNumber,
                               &errStatusSec));
    }

    constexpr UINT32 expectedStatusValue = 0x2;
    if ((priPadlinkNum != ILWALID_PADLINK_NUM && (errStatusPri != expectedStatusValue))
            || (secPadlinkNum != ILWALID_PADLINK_NUM && (errStatusSec != expectedStatusValue)))
    {
        UINT32 buffer[5] = { };
        Printf(Tee::PriDebug, "-------------------------------------------------------\n");
        
        CHECK_RC(ReadIobistReg(MODS_DISP_DUALIO_BIST_CBUF_0, sorNumber, &buffer[0]));
        CHECK_RC(ReadIobistReg(MODS_DISP_DUALIO_BIST_CBUF_1, sorNumber, &buffer[1]));
        CHECK_RC(ReadIobistReg(MODS_DISP_DUALIO_BIST_CBUF_2, sorNumber, &buffer[2]));
        CHECK_RC(ReadIobistReg(MODS_DISP_DUALIO_BIST_CBUF_3, sorNumber, &buffer[3]));
        CHECK_RC(ReadIobistReg(MODS_DISP_DUALIO_BIST_CBUF_4, sorNumber, &buffer[4]));

        Printf(Tee::PriError,
                "Expected value %u, Primary link value %u, Secondary link value %u "
                "for Sor = %u, lane = %u. "
                "Value from capture buffer for sublink %u:\n"
                "    0 : 0x%x (%u)\n"
                "    1 : 0x%x (%u)\n"
                "    2 : 0x%x (%u)\n"
                "    3 : 0x%x (%u)\n"
                "    4 : 0x%x (%u)\n",
                expectedStatusValue,
                priPadlinkNum == ILWALID_PADLINK_NUM ? expectedStatusValue : errStatusPri,
                secPadlinkNum == ILWALID_PADLINK_NUM ? expectedStatusValue : errStatusSec,
                sorNumber,
                lane,
                sorSublink,
                buffer[0], buffer[0],
                buffer[1], buffer[1],
                buffer[2], buffer[2],
                buffer[3], buffer[3],
                buffer[4], buffer[4]);
        return RC::SOR_CRC_MISCOMPARE;
    }

    string padlinkStr;
    Printf(Tee::PriDebug,
            "Test complete for sorNumber = %u, padlink(s) = %c%c input lane = %u\n",
            sorNumber,
            (priPadlinkNum != ILWALID_PADLINK_NUM) ? (priPadlinkNum + 'A') : ' ',
            (secPadlinkNum != ILWALID_PADLINK_NUM) ? (secPadlinkNum + 'A') : ' ',
            lane);

    return rc;
}

RC IobistLoopbackTester::SelectPRBSPattern(UINT32 sorNumber, UINT32 sorSublink)
{
    RC rc;

    // Set the seed for both lanes
    // TODO anahar: use arg for seed
    CHECK_RC(SetSeed(sorNumber, sorSublink));

    // Load the seed
    CHECK_RC(LoadSeed(sorNumber));
   
    // Select PRBS mode
    CHECK_RC(SelectPRBSMode(sorNumber, sorSublink));

    return rc;
}

RC IobistLoopbackTester::SetSeed(UINT32 sorNumber, UINT32 sorSublink)
{
    RC rc;

    if (sorSublink == 0)
    {
        UINT32 regData = 0;
        CHECK_RC(ReadIobistReg(MODS_DISP_DUALIO_BIST_LANE_00_PRBS, sorNumber, &regData));

        m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_LANE_00_PRBS_DPG_PRBSSEED, 100);

        CHECK_RC(WriteIobistReg(MODS_DISP_DUALIO_BIST_LANE_00_PRBS, sorNumber, regData));
    }
    else
    {
        UINT32 regData = 0;
        CHECK_RC(ReadIobistReg(MODS_DISP_DUALIO_BIST_LANE_01_PRBS, sorNumber, &regData));

        m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_LANE_01_PRBS_DPG_PRBSSEED, 100);

        CHECK_RC(WriteIobistReg(MODS_DISP_DUALIO_BIST_LANE_01_PRBS, sorNumber, regData));
    }

    return rc;
}

RC IobistLoopbackTester::LoadSeed(UINT32 sorNumber)
{
    RC rc;

    UINT32 regData = 0;
    CHECK_RC(ReadIobistReg(MODS_DISP_DUALIO_BIST_CONFIG, sorNumber, &regData));

    m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_CONFIG_DPG_PRBSSEEDLD, 0x1);

    CHECK_RC(WriteIobistReg(MODS_DISP_DUALIO_BIST_CONFIG, sorNumber, regData));
    
    regData = 0;
    CHECK_RC(ReadIobistReg(MODS_DISP_DUALIO_BIST_CONFIG, sorNumber, &regData));

    m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_CONFIG_DPG_PRBSSEEDLD, 0x0);

    CHECK_RC(WriteIobistReg(MODS_DISP_DUALIO_BIST_CONFIG, sorNumber, regData));

    return rc;
}

RC IobistLoopbackTester::SelectPRBSMode(UINT32 sorNumber, UINT32 sorSublink)
{
    RC rc;

    if (sorSublink == 0)
    {
        UINT32 regData = 0;
        CHECK_RC(ReadIobistReg(MODS_DISP_DUALIO_BIST_LANE_00_CONFIG, sorNumber, &regData));

        m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_LANE_00_CONFIG_DPG_DATASEL, 4);
        m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_LANE_00_CONFIG_DPV_DATASEL, 4);

        CHECK_RC(WriteIobistReg(MODS_DISP_DUALIO_BIST_LANE_00_CONFIG, sorNumber, regData));
    }
    else
    {
        UINT32 regData = 0;
        CHECK_RC(ReadIobistReg(MODS_DISP_DUALIO_BIST_LANE_01_CONFIG, sorNumber, &regData));

        m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_LANE_01_CONFIG_DPG_DATASEL, 4);
        m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_LANE_01_CONFIG_DPV_DATASEL, 4);

        CHECK_RC(WriteIobistReg(MODS_DISP_DUALIO_BIST_LANE_01_CONFIG, sorNumber, regData));
    }

    return rc;
}

RC IobistLoopbackTester::ConfigureSubLink
(
 UINT32 sorNumber,
 INT32 padlinkNum,
 UINT32 sorSublink
 )
{
    RC rc;

        if (padlinkNum == ILWALID_PADLINK_NUM)
        {
            return rc;
        }

        if (!m_pRegHal->IsSupported(MODS_PDISP_FE_CMGR_CLK_LINK_CTRL, padlinkNum))
        {
            Printf(Tee::PriError, "Register unsupported %s for padlinkNum %u\n",
                    m_pRegHal->ColwertToString(MODS_PDISP_FE_CMGR_CLK_LINK_CTRL), padlinkNum);
            return RC::SOFTWARE_ERROR;
        }

        const UINT32 regValue
            = m_pRegHal->SetField(MODS_PDISP_FE_CMGR_CLK_LINK_CTRL_FRONTEND, sorNumber + 1)
            | m_pRegHal->SetField(MODS_PDISP_FE_CMGR_CLK_LINK_CTRL_FRONTEND_SOR, sorSublink);

        m_LwDispRegHal->Write32(MODS_PDISP_FE_CMGR_CLK_LINK_CTRL, padlinkNum,
                regValue);

        Printf(Tee::PriDebug, "0x%x written to register(padlinkNum) %s(%u)\n",
                regValue, m_pRegHal->ColwertToString(MODS_PDISP_FE_CMGR_CLK_LINK_CTRL), sorSublink);

    return rc;
}

RC IobistLoopbackTester::CleanupRegisters(UINT32 sorNumber)
{
    RC rc;

    Printf(Tee::PriDebug, "Starting IOBIST Cleanup\n");
    UINT32 regData = 0;
    CHECK_RC(ReadIobistReg(MODS_DISP_DUALIO_BIST_CONFIGREG_2, sorNumber, &regData));

    m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_CONFIGREG_2_FIFO_WRITE_EN_PRESYNC_IN, 0);
    m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_CONFIGREG_2_FIFO_READ_EN_PRESYNC_IN, 0);

    CHECK_RC(WriteIobistReg(MODS_DISP_DUALIO_BIST_CONFIGREG_2, sorNumber, regData));

    regData = 0;
    CHECK_RC(ReadIobistReg(MODS_DISP_DUALIO_BIST_CONFIGREG, sorNumber, &regData));

    m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_CONFIGREG_DPG_TXDATAMUXSEL_IN, 0);
    m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_CONFIGREG_RATE_OVERRIDE_IN, 0);

    CHECK_RC(WriteIobistReg(MODS_DISP_DUALIO_BIST_CONFIGREG, sorNumber, regData));

    regData = 0;
    CHECK_RC(ReadIobistReg(MODS_DISP_DUALIO_BIST_CONFIG, sorNumber, &regData));

    m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_CONFIG_DPV_CAPTUREEN, 0);
    m_pRegHal->SetField(&regData, MODS_DISP_DUALIO_BIST_CONFIG_CFGCLKGATEEN, 0);

    CHECK_RC(WriteIobistReg(MODS_DISP_DUALIO_BIST_CONFIG, sorNumber, regData));

    return rc;
}

RC IobistLoopbackTester::WriteIobistReg
(
    ModsGpuRegAddress regAddr,
    UINT32 sorNumber,
    UINT32 data
)
{
    RC rc;

    // Check if internal register is supported
    if (!m_pRegHal->IsSupported(regAddr))
    {
        Printf(Tee::PriError, "Write to internal register failed, register not supported %s\n",
               m_pRegHal->ColwertToString(regAddr));
        return RC::REGISTER_READ_WRITE_FAILED;
    }

    // Fill in the data register
    // This should happen before we write to MODS_PDISP_SOR_IOBIST_CMD register
    m_LwDispRegHal->Write32(MODS_PDISP_SOR_IOBIST_DATA, sorNumber, data);

    // Write command register with operation and register address
    const UINT32 cmd
        = m_pRegHal->SetField(MODS_PDISP_SOR_IOBIST_CMD_OPCODE_WRITE)
        | m_pRegHal->SetField(MODS_PDISP_SOR_IOBIST_CMD_ADDRESS,
                        m_pRegHal->LookupAddress(regAddr));
    m_LwDispRegHal->Write32(MODS_PDISP_SOR_IOBIST_CMD, sorNumber, cmd);

    // Check if internal register access has caused any error
    if (m_LwDispRegHal->Test32(MODS_PDISP_SOR_IOBIST_CMD_ERROR_YES, sorNumber))
    {
        Printf(Tee::PriError, "Unable to write data to internal register %s\n",
               m_pRegHal->ColwertToString(regAddr));
        return RC::REGISTER_READ_WRITE_FAILED;
    }

    Printf(Tee::PriDebug, "0x%x written to register %s(%u)\n",
           data, m_pRegHal->ColwertToString(regAddr), sorNumber);

    return rc;
}

RC IobistLoopbackTester::ReadIobistReg
(
    ModsGpuRegAddress regAddr,
    UINT32 sorNumber,
    UINT32 *pData
)
{
    RC rc;
    MASSERT(pData);

    // Check if internal register is supported
    if (!m_pRegHal->IsSupported(regAddr))
    {
        Printf(Tee::PriError, "Read from internal register failed, register not supported %s\n",
               m_pRegHal->ColwertToString(regAddr));
        return RC::REGISTER_READ_WRITE_FAILED;
    }

    // Write command register with operation and register address
    const UINT32 cmd
        = m_pRegHal->SetField(MODS_PDISP_SOR_IOBIST_CMD_OPCODE_READ)
        | m_pRegHal->SetField(MODS_PDISP_SOR_IOBIST_CMD_ADDRESS,
                        m_pRegHal->LookupAddress(regAddr));
    m_LwDispRegHal->Write32(MODS_PDISP_SOR_IOBIST_CMD, sorNumber, cmd);

    // Check if internal register access has caused any error
    if (m_LwDispRegHal->Test32(MODS_PDISP_SOR_IOBIST_CMD_ERROR_YES, sorNumber))
    {
        Printf(Tee::PriError, "Unable to read data from internal register %s\n",
               m_pRegHal->ColwertToString(regAddr));
        return RC::REGISTER_READ_WRITE_FAILED;
    }

    // Read back the actual data
    *pData = m_LwDispRegHal->Read32(MODS_PDISP_SOR_IOBIST_DATA, sorNumber);

    Printf(Tee::PriDebug, "Data 0x%x read from register %s(%u)\n",
           *pData, m_pRegHal->ColwertToString(regAddr), sorNumber);

    return rc;
}

RC LoopbackTester::RegisterDoWars(DoWarsCB cb)
{
    if (!cb)
    {
        Printf(Tee::PriError, "Provided callback is not callable\n");
        return RC::ILWALID_ARGUMENT;
    }

    m_DoWarsCB = cb;
    return RC::OK;
}

RC LoopbackTester::RegisterIsTestRequiredForDisplay(IsTestRequiredForDisplayCB cb)
{
    if (!cb)
    {
        Printf(Tee::PriError, "Provided callback is not callable\n");
        return RC::ILWALID_ARGUMENT;
    }

    m_IsTestRequiredForDisplayCB = cb;
    return RC::OK;
}
