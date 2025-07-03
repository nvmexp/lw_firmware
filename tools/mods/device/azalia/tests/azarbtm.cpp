/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2014,2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "azarbtm.h"
#include "device/azalia/azacdc.h"
#include "device/azalia/azactrl.h"
#include "device/azalia/azareg.h"
#include "device/azalia/azafpci.h"
#include "core/include/platform.h"
#include "cheetah/include/logmacros.h"
#include "cheetah/include/tegradrf.h"

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME "AzaRBTM"

#define RB_NO_RESPONSE_EXPECTED ((UINT64) ~(0x0))

#define RB_DEFAULT_NCOMMANDS           512
#define RB_DEFAULT_CORBSIZE            256
#define RB_DEFAULT_RIRBSIZE            256
#define RB_DEFAULT_RINTCNT               1
#define RB_DEFAULT_CORBSTOP          false
#define RB_DEFAULT_RIRBSTOP          false
#define RB_DEFAULT_ALLOWPTRRESETS     true
#define RB_DEFAULT_ALLOWNULLCOMMANDS  true
#define RB_PAUSE_MIN_DUR              2000 // usec
#define RB_MAX_COMMANDS_BEFORE_PAUSE   399

AzaliaRingBufferTestModule::AzaliaRingBufferTestModule()
: AzaliaTestModule()
{
    m_RegReadOnlyTable.clear();
    m_RegReadWriteTable.clear();
    m_Status = {};
    m_Status.pushedVerbs = new queue<UINT32>;
    m_Status.expectedResponses = new queue<UINT64>;
    MASSERT(m_Status.pushedVerbs && m_Status.expectedResponses);
    SetDefaults();
}

AzaliaRingBufferTestModule::~AzaliaRingBufferTestModule()
{
    delete m_Status.pushedVerbs;
    delete m_Status.expectedResponses;
}

RC AzaliaRingBufferTestModule::DoPrintInfo(Tee::Priority Pri, bool PrintInput, bool PrintStatus)
{
    if (m_IsEnabled && PrintInput)
    {
        Printf(Pri, "Ring Buffer Test (%s)\n", (m_IsEnabled ? "enabled" : "disabled"));
        Printf(Pri, "    JS Variable                    Value  Description\n");
        Printf(Pri, "    -----------                    -----  -----------\n");
        Printf(Pri, "    Properties that are set by calling Reset\n");
        Printf(Pri, "    -----------------------------------------\n");
        Printf(Pri, "    RB_CorbSize                  : %5d  Size of CORB\n", m_CorbSize);
        Printf(Pri, "    RB_RirbSize                  : %5d  Size of RIRB\n", m_RirbSize);
        Printf(Pri, "    RB_CorbStop                  : %5d  Tests pausing the CORB\n", m_IsCorbStop);
        Printf(Pri, "    RB_RirbStop                  : %5d  Tests pausing the RIRB\n", m_IsRirbStop);
        Printf(Pri, "    RB_NCommands                 : %5d  Number of commands to send\n", m_NumCommands);
        Printf(Pri, "    RB_AllowPtrResets            : %5d  Allow the CORB/RIRB pointers to be reset\n", m_IsAllowPtrResets);
        Printf(Pri, "    RB_AllowNullCommands         : %5d  Allow null commands to be sent to the codec\n", m_IsAllowNullCommands);
        Printf(Pri, "    RB_RIntCnt                   : %5d  Value for the RIntCnt register\n", m_RIntCnt);
        Printf(Pri, "    RB_ROIntEn                   : %5d  Enable RIRB Overrun interrupts\n", m_IsRirbOIntEn);
        Printf(Pri, "    RB_RRIntEn                   : %5d  Enable RIRB Response interrupts\n", m_IsRirbRIntEn);
        Printf(Pri, "    RB_OverrunTest               : %5d  Run the RIRB Overrun test\n", m_IsEnableOverrunTest);
        Printf(Pri, "    RB_EnableUnsolResponses      : %5d  Enable unsolicited responses on the controller\n", m_IsUnsolRespEnable);
        Printf(Pri, "    RB_ExpectUnsolResponses      : %5d  Expect unsolicited responses \n", m_IsExpUnsolResp);
        Printf(Pri, "    RB_EnableCodelwnsolResponses : %5d  Enable unsolicited responses on the codec\n", m_IsUnsolRespEnableCdc);
        Printf(Pri, "    -----------------------------------------\n");
        Printf(Pri, "    Properties that can be changed on the fly \n");
        Printf(Pri, "    -----------------------------------------\n");
        Printf(Pri, "    RB_Enable                    : %5d  Enable the Ring Buffer test\n", m_IsEnabled);
        Printf(Pri, "\n");
    }

    if (m_IsEnabled && PrintStatus)
    {
        Printf(Pri, "RB test: %d + %ld(%ld) = %d <= %d, Finished: %d, Has Err: %d\n",
               m_Status.commandsFinished,
               static_cast<long>(m_Status.pushedVerbs->size()),
               static_cast<long>(m_Status.expectedResponses->size()),
               m_Status.commandsStarted,
               m_NumCommands, m_IsFinished, m_IsError);
    }

    return OK;
}

RC AzaliaRingBufferTestModule::Reset(AzaliaController* pAza)
{
    RC rc;
    UINT16 size;

    CHECK_RC(ResetBase(pAza));

    // Set CORB and RIRB to proper sizes and start them running
    CHECK_RC(m_pAza->CorbInitialize(m_CorbSize));
    CHECK_RC(m_pAza->CorbGetTotalSize(&size));
    if (size != m_CorbSize)
    {
        Printf(Tee::PriError, "Cannot set CORB to proper size! Wanted %d, got %d.\n",
               m_CorbSize, size);
        return RC::DATA_MISMATCH;
    }

    CHECK_RC(m_pAza->RirbInitialize(m_RirbSize, m_RIntCnt, m_IsRirbRIntEn, m_IsRirbOIntEn));
    CHECK_RC(m_pAza->RirbGetTotalSize(&size));
    if (size != m_RirbSize)
    {
        Printf(Tee::PriError, "Cannot set CORB to proper size! Wanted %d, got %d.\n",
               m_RirbSize, size);
        return RC::DATA_MISMATCH;
    }
    CHECK_RC(m_pAza->RirbToggleRunning(true));
    CHECK_RC(m_pAza->CorbToggleRunning(true));

    // Reset internal state variables
    while (!m_Status.pushedVerbs->empty())
        m_Status.pushedVerbs->pop();
    while (!m_Status.expectedResponses->empty())
        m_Status.expectedResponses->pop();
    queue<UINT32>* tempVerbs = m_Status.pushedVerbs;
    queue<UINT64>* tempResponses = m_Status.expectedResponses;
    memset(&(m_Status), 0, sizeof(m_Status));
    m_Status.pushedVerbs = tempVerbs;
    m_Status.expectedResponses = tempResponses;
    m_Status.corbInfo.commandsUntilPause =
        AzaliaUtilities::GetRandom(0, RB_MAX_COMMANDS_BEFORE_PAUSE);
    m_Status.rirbInfo.commandsUntilPause =
        AzaliaUtilities::GetRandom(0, RB_MAX_COMMANDS_BEFORE_PAUSE);
    m_Status.rirbOverrunStatus = m_IsEnableOverrunTest ? DO_TEST : DO_NOT_TEST;
    m_Status.busMasterDisabled = false;
    m_RegReadOnlyTable.clear();
    m_RegReadWriteTable.clear();

    // Prepare internal data structures
    if (m_IsEnabled)
    {
        CHECK_RC(PrepareRegisterTables());
        AzaliaController::IntControllerStatus intStatus;
        CHECK_RC(m_pAza->IntControllerGetStatus(&intStatus));
        CHECK_RC(m_pAza->IntControllerClearStatus());

        if (m_IsUnsolRespEnableCdc)
        {
            AzaliaCodec* pCodec;
            for (UINT08 codec = 0; codec < MAX_NUM_SDINS; ++codec)
            {
                if ( (m_pAza->GetCodec(codec, &pCodec) == OK) &&
                     (pCodec != NULL))
                {
                    CHECK_RC( pCodec->EnableUnsolicitedResponses(true) );
                }
            }
        }

        CHECK_RC(m_pAza->ToggleUnsolResponses(m_IsUnsolRespEnable));
    }

    return OK;
}

RC AzaliaRingBufferTestModule::PrepareRegisterTables()
{
    LOG_ENT();
    RC rc;
    AzaliaCodec* pCodec = NULL;
    RegTestInfo r;
    m_RegReadOnlyTable.clear();
    m_RegReadWriteTable.clear();
    for (UINT08 i=0; i<MAX_NUM_SDINS; i++)
    {
        if ( (m_pAza->GetCodec(i, &pCodec) != OK) ||
             (pCodec == NULL))
        {
            continue;
        }
        r.codecAddr = i;

        // Query root node and get vendor ID info
        UINT16 vendorID;
        UINT16 deviceID;
        CHECK_RC(pCodec->GetVendorDeviceID(&vendorID, &deviceID));
        r.nodeID = 0;
        r.verb = VERB_GET_PARAMETER;
        r.payload = AZA_CDCP_VENDOR_ID;
        r.response = RF_NUM(AZA_CDCP_VENDOR_ID_VID, vendorID) |
                     RF_NUM(AZA_CDCP_VENDOR_ID_DID, deviceID);
        m_RegReadOnlyTable.push_back(r);

        r.payload = AZA_CDCP_REV_ID;
        CHECK_RC(pCodec->SendCommand(r.nodeID, r.verb, r.payload, &r.response));
        m_RegReadOnlyTable.push_back(r);

        UINT32 nFuncGroups = 0;
        UINT32 groupStartNode = 0;
        r.payload = AZA_CDCP_SUB_NODE_CNT;
        CHECK_RC(pCodec->SendCommand(r.nodeID, r.verb, r.payload, &r.response));
        m_RegReadOnlyTable.push_back(r);
        nFuncGroups = RF_VAL(AZA_CDCP_SUB_NODE_CNT_TOTAL, r.response);
        groupStartNode = RF_VAL(AZA_CDCP_SUB_NODE_CNT_START, r.response);

        for (UINT32 fgCnt=0; fgCnt<nFuncGroups; fgCnt++)
        {
            // Query functional group
            r.nodeID = fgCnt + groupStartNode;
            r.verb = VERB_GET_PARAMETER;
            r.payload = AZA_CDCP_FGROUP_TYPE;
            CHECK_RC(pCodec->SendCommand(r.nodeID, r.verb, r.payload, &r.response));
            m_RegReadOnlyTable.push_back(r);

            if ((vendorID != AZA_CDC_VENDOR_LWIDIA) &&
                (vendorID != AZA_CDC_VENDOR_ADI || deviceID != AZA_CDC_ADI_1986))
            {
                // No subsystem ID on LW internal codec or on ADI1986
                r.verb = VERB_GET_SUBSYSTEM_ID;
                r.payload = 0;
                CHECK_RC(pCodec->SendCommand(r.nodeID, r.verb, r.payload, &r.response));
                m_RegReadWriteTable.push_back(r);
            }

            UINT32 nWidgets = 0;
            UINT32 startWidgetNode = 0;
            r.verb = VERB_GET_PARAMETER;
            r.payload = AZA_CDCP_SUB_NODE_CNT;
            CHECK_RC(pCodec->SendCommand(r.nodeID, r.verb, r.payload, &r.response));
            m_RegReadOnlyTable.push_back(r);
            nWidgets = RF_VAL(AZA_CDCP_SUB_NODE_CNT_TOTAL, r.response);
            startWidgetNode = RF_VAL(AZA_CDCP_SUB_NODE_CNT_START, r.response);

            for (UINT32 wCnt=0; wCnt<nWidgets; wCnt++)
            {
                r.nodeID = startWidgetNode + wCnt;
                r.verb = VERB_GET_PARAMETER;
                r.payload = AZA_CDCP_AUDIOW_CAP;
                CHECK_RC(pCodec->SendCommand(r.nodeID, r.verb, r.payload, &r.response));
                m_RegReadOnlyTable.push_back(r);

                UINT32 wType = RF_VAL(AZA_CDCP_AUDIOW_CAP_TYPE, r.response);
                if (wType == AZA_CDCP_AUDIOW_CAP_TYPE_PIN)
                {
                    r.verb = VERB_GET_CONFIG_DEFAULT;
                    r.payload = 0;
                    CHECK_RC(pCodec->SendCommand(r.nodeID, r.verb, r.payload, &r.response));
                    m_RegReadWriteTable.push_back(r);
                }
            }

        }
    }

    if (m_RegReadOnlyTable.size() == 0)
    {
        Printf(Tee::PriError, "RegReadOnlyTable is empty after construction!\n");
        return RC::SOFTWARE_ERROR;
    }
    if (m_RegReadWriteTable.size() == 0)
    {
        Printf(Tee::PriError, "RegReadWriteTable is empty after construction!\n");
        return RC::SOFTWARE_ERROR;
    }

    LOG_EXT();
    return OK;
}

RC AzaliaRingBufferTestModule::Start()
{
    RC rc;
    // Starting is the same as continuing...
    CHECK_RC(Continue());

    return OK;
}

RC AzaliaRingBufferTestModule::Continue()
{
    RC rc;
    LOG_ENT();
    if (m_IsEnabled)
    {
        if (m_Status.rirbOverrunStatus > DO_NOT_TEST)
        {
            if (m_Status.rirbOverrunStatus < IN_PROGRESS)
            {
                // NOTE: there is a  race condition here.  Turning on bus mastering
                // allows the running RIRB to transfer data to memory, making a RIRB
                // overflow harder to produce.  Unfortunately, we have to turn on
                // bus mastering to allow us to add data to the CORB.  It's important
                // to stop the CORB and RIRB as soon as possible after turning on
                // bus mastering.  It's not possible to turn off the CORB and RIRB
                // without turning on bus mastering, and the CORB must be stopped
                // before the RIRB to avoid losing responses and therefore causing
                // mismatches. At the moment, the timing is quick enough that
                // we still end up producing an overflow.
                Printf(Tee::PriDebug, "Enabling Bus Mastering...\n");
                m_Status.busMasterDisabled = false;
                CHECK_RC(m_pAza->CfgWr08(AZA_CFG_1, 0x07));
                CHECK_RC(ToggleCorbRunning(false));
                CHECK_RC(ToggleRirbRunning(false));

                UINT16 nCommands;
                CHECK_RC(m_pAza->CorbGetFreeSpace(&nCommands));
                m_Status.chunkSize = nCommands - 1;

                CHECK_RC(PushCommands());
                m_Status.commandsStarted += m_Status.chunkSize;
                m_Status.rirbOverrunStatus = IN_PROGRESS;

                // NOTE: again, another race condition.  See note above, at the beginning
                // of this code block.
                Printf(Tee::PriDebug, "Disabling Bus Mastering...\n");
                CHECK_RC(ToggleRirbRunning(true));
                CHECK_RC(ToggleCorbRunning(true));

                // On simulation the timing issues results in the RIRB Overrun
                // not oclwrring. Hence we need to disable writes only in one
                // direction by writing to the XVE in GT21x to disable memory
                // accesses only in one direction
                #ifdef SIM_BUILD
                if (m_pAza->IsGpuAza())
                {
                    // Workaround by writing to XVE to disable writes
                    // only in one direction
                    UINT32 index = 0;
                    UINT32 gpuDomain, gpuBus, gpuDev, gpuFunc;
                    UINT32 data = 0;
                    Printf(Tee::PriDebug, "Finding GPU Do/B/D/F\n");
                    while (OK == Platform::FindPciClassCode(0x030000, //Pci::CLASS_CODE_VIDEO_CONTROLLER_VGA,
                                           index++, &gpuDomain, &gpuBus, &gpuDev, &gpuFunc));
                    Printf(Tee::PriDebug, "GPU found at Do: %d, B: %d, D: %d, F: %d\n",
                           gpuDomain, gpuBus, gpuDev, gpuFunc);

                    Printf(Tee::PriDebug, "Reading GPU Bar\n");
                    CHECK_RC(Platform::PciRead32(gpuDomain, gpuBus, gpuDev, gpuFunc, 0x10,
                                                 &data));

                    m_Status.physGpuBar0 = data & 0xfffffff0;
                    Printf(Tee::PriDebug, "GPU Bar 0 = 0x%08x\n", m_Status.physGpuBar0);

                    void *vaddr = NULL;
                    vaddr = Platform::PhysicalToVirtual(m_Status.physGpuBar0 + 0x8A004);
                    data = MEM_RD32(vaddr);
                    Printf(Tee::PriDebug, "Offset 0x8A004 = 0x%08x\n", data);
                    m_Status.xveFifoWrLimit = data;
                    data = FLD_SET_RF_NUM(2:2, 0, data);
                    Printf(Tee::PriDebug, "Writing Offset 0x8A004 = 0x%08x\n", data);
                    MEM_WR32(vaddr, data);
                    Printf(Tee::PriDebug, "Write Done\n");
                }
                else
                    CHECK_RC(m_pAza->CfgWr08(AZA_CFG_1, 0x03));
                #else
                CHECK_RC(m_pAza->CfgWr08(AZA_CFG_1, 0x03));
                #endif
                m_Status.busMasterDisabled = true;

                // TODO: without this sleep, the overflow never oclwrs when running
                // without debug spew.  Why?
                AzaliaUtilities::Pause(1);
            }
            else if (m_Status.rirbOverrunStatus == COMPLETED_PASS)
            {
                if (m_Status.busMasterDisabled)
                {
                    // bus mastering is still off.  We need to turn it back on
                    // and clear out the CORB and RIRB so the RB test can continue
                    Printf(Tee::PriDebug, "Enabling Bus Mastering...\n");
                    m_Status.busMasterDisabled = false;

                    if (m_pAza->IsGpuAza())
                    {
                        void *vaddr = NULL;
                        vaddr = Platform::PhysicalToVirtual(m_Status.physGpuBar0 + 0x8A004);
                        MEM_WR32(vaddr,m_Status.xveFifoWrLimit);
                        Printf(Tee::PriDebug, "Restoring XVE Register\n");
                    }
                    else
                    {
                        AzaliaController::IntrModes mode;
                        CHECK_RC(m_pAza->GetInterruptMode(&mode));
                        if (mode == AzaliaController::POLL)
                        {
                            Printf(Tee::PriDebug, "Enabling bus-mastering in POLL Mode\n");
                            CHECK_RC(m_pAza->CfgWr08(AZA_CFG_1, 0x07));
                        }
                    }

                    // Clear out the CORB
                    Printf(Tee::PriDebug, "Clearing out the CORB...\n");
                    CHECK_RC(ToggleCorbRunning(false));
                    CHECK_RC(m_pAza->CorbReset());
                    CHECK_RC(ToggleCorbRunning(true));
                    AzaliaUtilities::Pause(10);

                    // Clear out the RIRB
                    Printf(Tee::PriDebug, "Clearing out the RIRB...\n");
                    CHECK_RC(ToggleRirbRunning(false));
                    CHECK_RC(m_pAza->RirbReset());
                    CHECK_RC(ToggleRirbRunning(true));

                    // We didn't clear the CORB and RIRB when we detected the
                    // overrun before, so there has likely been another overrun,
                    // and we need to clear the overrun flag.
                    UINT32 rirbSts;
                    CHECK_RC(m_pAza->RegRd(RIRBSTS, &rirbSts, true));

                    Printf(Tee::PriDebug, "Clearing the overrun flag...\n");
                    if (FLD_TEST_RF_DEF(AZA_RIRBCTL_RIRBOIS, _OVERRUN, rirbSts))
                    {
                        rirbSts = 0;
                        rirbSts = FLD_SET_RF_DEF(AZA_RIRBCTL_RIRBOIS, _W1C, rirbSts);
                        CHECK_RC(m_pAza->RegWr(RIRBSTS, rirbSts));
                    }

                    while (!m_Status.expectedResponses->empty())
                    {
                        m_Status.expectedResponses->pop();
                    }
                    m_Status.commandsStarted = 0;
                    m_Status.commandsFinished = 0;
                    m_Status.chunkSize = 0;

                    m_RegReadOnlyTable.clear();
                    m_RegReadWriteTable.clear();
                    CHECK_RC(PrepareRegisterTables());
                    return OK;
                }
                else
                {
                    // Bus mastering has already been turned on--we must have been
                    // through this loop at least once.  Just return
                    m_IsFinished = true;
                    return OK;
                }
            }
        }

        CHECK_RC(ProcessResponses());
        CHECK_RC(SendCommands());
        CHECK_RC(PrintInfo(Tee::PriLow, false, true));

    } else {
        // Simple check to make sure nothing has gone wrong
        AzaliaController::IntControllerStatus intStatus;
        CHECK_RC(m_pAza->IntControllerGetStatus(&intStatus));
        CHECK_RC(m_pAza->IntControllerClearStatus());
        if (intStatus.corbMemErr)
        {
            Printf(Tee::PriError, "CORB memory error!\n");
            m_IsError = true;
        }
        if (intStatus.rirbOverrun)
        {
            Printf(Tee::PriError, "RIRB overrun detected\n");
            m_IsError = true;
        }

        m_IsFinished = true;
    }
    return OK;
}

RC AzaliaRingBufferTestModule::Stop()
{
    // Check if we received unsolicited when they were expected
    if (m_IsExpUnsolResp)
    {
        if (m_Status.numUnsolRespReceived == 0)
        {
            Printf(Tee::PriHigh, "No unsolicited responses were received "
                                 "while they were expected!\n");
            m_IsError = true;
        }
        else
            Printf(Tee::PriDebug, "%d unsolicited responses were received\n",
                   m_Status.numUnsolRespReceived);
    }
    return OK;
}

RC AzaliaRingBufferTestModule::SetDefaults()
{
    RC rc;
    CHECK_RC(SetDefaultsBase());
    m_NumCommands = RB_DEFAULT_NCOMMANDS;
    m_CorbSize = RB_DEFAULT_CORBSIZE;
    m_RirbSize = RB_DEFAULT_RIRBSIZE;
    m_RIntCnt = RB_DEFAULT_RINTCNT;
    m_IsRirbOIntEn = false;
    m_IsRirbRIntEn = false;
    m_IsCorbStop = RB_DEFAULT_CORBSTOP;
    m_IsRirbStop = RB_DEFAULT_RIRBSTOP;
    m_IsAllowPtrResets = RB_DEFAULT_ALLOWPTRRESETS;
    m_IsAllowNullCommands = RB_DEFAULT_ALLOWNULLCOMMANDS;
    m_IsEnableOverrunTest = false;
    m_IsExpUnsolResp = false;
    m_IsUnsolRespEnable = false;
    m_IsUnsolRespEnableCdc = false;
    return OK;
}

RC AzaliaRingBufferTestModule::ToggleCorbRunning(bool IsRun)
{
    RC rc;
    if (IsRun)
    {
        if (m_Status.corbInfo.stopped)
        {
            if (m_IsAllowPtrResets
                && m_Status.commandsStarted == m_Status.commandsFinished
                && !AzaliaUtilities::GetRandom(0,1))
            {
                // Obviously, we only want to reset the pointers if we've
                // finished processing all of the current commands.
                // Otherwise, we risk losing some commands!
                Printf(Tee::PriLow, "Resetting the CORB pointers\n");
                CHECK_RC(m_pAza->CorbReset());
            }

            MASSERT(!m_Status.rirbInfo.stopped);
            CHECK_RC(m_pAza->CorbToggleRunning(true));
            m_Status.corbInfo.stopped = false;
        }
        else
        {
            Printf(Tee::PriDebug, "CORB is already running.\n");
        }
    }
    else
    {
        if (!m_Status.corbInfo.stopped)
        {
            CHECK_RC(m_pAza->CorbToggleRunning(false));
            m_Status.corbInfo.stopped = true;
        }
        else
        {
            Printf(Tee::PriDebug, "CORB is already stopped.\n");
        }
    }
    return OK;
}

RC AzaliaRingBufferTestModule::ToggleRirbRunning(bool IsRun)
{
    RC rc;
    if (IsRun)
    {
        if (m_Status.rirbInfo.stopped)
        {
            CHECK_RC(m_pAza->RirbToggleRunning(true));
            m_Status.rirbInfo.stopped = false;
        }
        else
        {
            Printf(Tee::PriDebug, "RIRB is already running.\n");
        }
    }
    else
    {
        if (!m_Status.rirbInfo.stopped)
        {
            //MASSERT(m_Status.corbInfo.stopped);
            CHECK_RC(m_pAza->RirbToggleRunning(false));
            m_Status.rirbInfo.stopped = true;
            Printf(Tee::PriLow, "Paused the RIRB.\n");

            if (m_IsAllowPtrResets
                && m_Status.commandsStarted == m_Status.commandsFinished
                && (AzaliaUtilities::GetRandom(0,3)==0))
            {
                // Obviously, we only want to reset the pointers if we've
                // finished processing all of the current commands.
                // Otherwise, we risk losing some commands!
                Printf(Tee::PriNormal, "Resetting the RIRB pointers.\n");
                CHECK_RC(m_pAza->RirbReset());
            }
            else if (m_IsAllowPtrResets)
            {
                if (m_Status.commandsStarted != m_Status.commandsFinished)
                {
                    Printf(Tee::PriNormal, "NOT resetting RIRB pointers because of commands!\n");
                }
                else
                {
                    Printf(Tee::PriNormal, "NOT resetting RIRB pointers BY CHANCE!\n");
                }
            }
        }
        else
        {
            Printf(Tee::PriDebug, "RIRB is already stopped.\n");
        }
    }
    return OK;
}

RC AzaliaRingBufferTestModule::GenerateBatchOfCommands(UINT32* Verbs,
                                                       UINT64* Responses,
                                                       UINT32 nCommands)
{
    RC rc;
    UINT32 nDone = 0;
    RegTestInfo* pInfo = NULL;
    while (nDone < nCommands)
    {
        UINT32 p = AzaliaUtilities::GetRandom(0, 10000);
        if (p < 4800)
        {
            if (AzaliaUtilities::GetRandom(0,1))
            {
                // Read a codec register from the read only list
                UINT32 index = AzaliaUtilities::GetRandom(0, m_RegReadOnlyTable.size() - 1);
                pInfo = &m_RegReadOnlyTable[index];
            }
            else
            {
                // Read a codec register from the read/write list
                UINT32 index = AzaliaUtilities::GetRandom(0, m_RegReadWriteTable.size() - 1);
                pInfo = &m_RegReadWriteTable[index];
            }
            CHECK_RC(AzaliaCodec::ConstructVerb(&(Verbs[nDone]), pInfo->codecAddr,
                                                pInfo->nodeID, pInfo->verb,
                                                pInfo->payload));
            Responses[nDone] = ((UINT64) pInfo->codecAddr << 32) | (UINT64) pInfo->response;
            nDone++;
        }
        else if (p < 9600)
        {
            // Write a codec register
            UINT32 payload = AzaliaUtilities::GetRandom(0,255);
            UINT32 index = AzaliaUtilities::GetRandom(0, m_RegReadWriteTable.size()-1);
            UINT32 verb = 0;
            UINT32 byte = 0;
            pInfo = &m_RegReadWriteTable[index];
            switch (pInfo->verb)
            {
                case VERB_GET_SUBSYSTEM_ID:
                    byte = AzaliaUtilities::GetRandom(0,3);
                    verb = VERB_SET_SUBSYSTEM_ID_BYTE0 + byte;
                    pInfo->response = (pInfo->response & ~(0xff << (8*byte))) |
                                       (payload << (8*byte));
                    break;
                case VERB_GET_CONFIG_DEFAULT:
                    byte = AzaliaUtilities::GetRandom(0,3);
                    verb = VERB_SET_CONFIG_DEFAULT_BYTE0 + byte;
                    pInfo->response = (pInfo->response & ~(0xff << (8*byte))) |
                                       (payload << (8*byte));
                    break;
                default:
                    Printf(Tee::PriError, "Unknown verb 0x%08x!\n", pInfo->verb);
                    MASSERT(!"Unknown verb");
                    return RC::SOFTWARE_ERROR;
            }
            CHECK_RC(AzaliaCodec::ConstructVerb(&(Verbs[nDone]), pInfo->codecAddr,
                                                pInfo->nodeID, verb, payload));
            Responses[nDone] = ((UINT64) pInfo->codecAddr << 32) | (UINT64) 0;
            nDone++;

            // Occasionally read back the value to confirm the write
            if (AzaliaUtilities::GetRandom(0,1) && (nDone < nCommands))
            {
                CHECK_RC(AzaliaCodec::ConstructVerb(&(Verbs[nDone]), pInfo->codecAddr,
                                                    pInfo->nodeID, pInfo->verb,
                                                    pInfo->payload));
                Responses[nDone] = ((UINT64) pInfo->codecAddr << 32) | (UINT64) pInfo->response;
                nDone++;
            }
        }
        else
        {
            // NULL command. These commands usually don't have a response, but
            // some codecs mistakenly send one.
            if (m_IsAllowNullCommands)
            {
                // Choose a random codec and node ID from an existing command
                UINT32 index = AzaliaUtilities::GetRandom(0, m_RegReadWriteTable.size()-1);
                pInfo = &m_RegReadWriteTable[index];
                CHECK_RC(AzaliaCodec::ConstructVerb(&(Verbs[nDone]), pInfo->codecAddr,
                                                    0, 0, 0));
                UINT16 vid = 0;
                UINT16 did = 0;
                AzaliaCodec* pCodec = NULL;
                CHECK_RC(m_pAza->GetCodec(pInfo->codecAddr, &pCodec));
                MASSERT(pCodec);
                CHECK_RC(pCodec->GetVendorDeviceID(&vid, &did));
                if ((vid == AZA_CDC_VENDOR_REALTEK) && (did == AZA_CDC_REALTEK_ALC880))
                {
                    // ALC880 sends a response to NULL commands
                    Responses[nDone] = ((UINT64)pInfo->codecAddr << 32);
                }
                else
                {
                    // By the spec, codecs should not respond to NULL Commands
                    Responses[nDone] = RB_NO_RESPONSE_EXPECTED;
                }
                nDone++;
            }
        }
    }
    return OK;
}

RC AzaliaRingBufferTestModule::PushCommands()
{
    RC rc;
    UINT16 nPushed = 0;
    vector<UINT32> verbs(m_Status.chunkSize);
    vector<UINT64> responses(m_Status.chunkSize);
    CHECK_RC(GenerateBatchOfCommands(&verbs[0], &responses[0], m_Status.chunkSize));
    CHECK_RC(m_pAza->CorbPush(&verbs[0], m_Status.chunkSize, &nPushed));
    Printf(Tee::PriDebug, "Push %d commands.\n", nPushed);
    if (nPushed != m_Status.chunkSize)
    {
        Printf(Tee::PriError, "CORB Push Failed!\n");
        return RC::AZA_RING_BUFFER_ERROR;
    }
    for (UINT32 i=0; i<m_Status.chunkSize; i++)
    {
        m_Status.pushedVerbs->push(verbs[i]);
        m_Status.expectedResponses->push(responses[i]);
    }
    return OK;
}

RC AzaliaRingBufferTestModule::ProcessResponses()
{
    RC rc;
    UINT16 nResponses = 0;
    UINT16 nPopped = 0;

    AzaliaController::IntControllerStatus intStatus;
    CHECK_RC(m_pAza->IntControllerGetStatus(&intStatus));
    CHECK_RC(m_pAza->RirbGetNResponses(&nResponses));
    CHECK_RC(m_pAza->IntControllerClearStatus());
    if (intStatus.corbMemErr)
    {
        Printf(Tee::PriError, "CORB memory error!\n");
        return RC::AZA_RING_BUFFER_ERROR;
    }
    if (intStatus.rirbOverrun)
    {
        if (m_Status.rirbOverrunStatus == IN_PROGRESS)
        {
            Printf(Tee::PriHigh, "Detected RIRB overrun at expected time\n");
            m_Status.rirbOverrunStatus = COMPLETED_PASS;
            return OK;
        }
        else
        {
            Printf(Tee::PriError, "RIRB overrun\n");
            return RC::AZATEST_FAIL;
        }
    }

    if (intStatus.rirbIntFlag)
    {
        // Ideally we'd verify that rirbIntFlag didn't trigger too soon
        // here.  However, it is hard to guarantee that there were no
        // open frames which would also trigger rirbIntFlag.

        if (nResponses)
        {
            vector<UINT64> responses(nResponses);
            CHECK_RC(m_pAza->RirbPop(&responses[0], nResponses, &nPopped));
            Printf(Tee::PriDebug, "Popped %d responses\n", nPopped);
            for (INT32 i=0; i<nPopped; i++)
            {
                UINT32 upperBits = responses[i] >> 32;
                bool unsol = RF_VAL(AZA_CDC_RESPONSE_UNSOL, upperBits) == AZA_CDC_RESPONSE_UNSOL_ISUNSOL;
                if (unsol)
                {
                    if (!m_IsUnsolRespEnable)
                    {
                        Printf(Tee::PriError, "Unsolicited response received while unsolicited"
                                              "responses are disabled in GCTL!\n");
                        m_IsError = true;
                    }
                    else if (!m_IsUnsolRespEnableCdc)
                    {
                        Printf(Tee::PriError, "Unsolicited response received while unsolicited"
                                              "responses are disabled in codec!\n");
                        m_IsError = true;
                    }
                    else if (!m_IsExpUnsolResp)
                    {
                        Printf(Tee::PriError, "An unsolicited response was received when none"
                                              "were expected\n");
                        m_IsError = true;
                    }
                    else
                    {
                        m_Status.numUnsolRespReceived++;

                        // Check the codec address
                        UINT08 codecAddr = (responses[i]>>32)&0x000000000000000F;
                        AzaliaCodec* pCodec;
                        UINT16 responseTag = 0;

                        CHECK_RC(m_pAza->GetCodec(codecAddr, &pCodec));
                        if (pCodec == NULL)
                        {
                            Printf(Tee::PriHigh, "Bad codec address!\n");
                            m_IsError = true;
                        }
                        else
                        {
                            UINT16 vendorId, deviceId;
                            CHECK_RC( pCodec->GetVendorDeviceID(&vendorId, &deviceId) );
                            if (vendorId == AZA_CDC_ADI_1986)
                                responseTag = (responses[i]>>0)&0x000000000000003F;  // ADI1986 does this wrong
                            else
                                responseTag = (responses[i]>>26)&0x000000000000003F;
                        }

                        vector<AzaliaRoute*> sourceRoutes;
                        pCodec->GetUnsolRespRoutes(true, true, &sourceRoutes, responseTag);

                        vector<AzaliaRoute*>::iterator routePtr;
                        for (routePtr = sourceRoutes.begin();
                             routePtr != sourceRoutes.end(); ++routePtr)
                        {
                            Printf(Tee::PriHigh, "Unsolicited response received on sdin %d "
                                                 "from %s pin %d.\n",
                                   codecAddr, (*routePtr)->IsInput()?"input":"output",
                                   (*routePtr)->LastNode());
                        }
                        if (sourceRoutes.size() == 0)
                        {
                            Printf(Tee::PriHigh, "Unsolicited response received on sdin %d "
                                                 "with tag 0x%02x but is an unknown source.\n",
                                   codecAddr, responseTag);
                            m_IsError = true;
                        }

                    }
                }
                else
                {
                    if (m_Status.expectedResponses->empty())
                    {
                        Printf(Tee::PriHigh, "Received response (0x%08x%08x) when response "
                                             "stack was empty!\n",
                               upperBits, (UINT32) responses[i]);

                        m_IsError = true;
                    }
                    else
                    {
                        UINT32 verb = m_Status.pushedVerbs->front();
                        UINT64 eResponse = m_Status.expectedResponses->front();

                        if (!RF_VAL(AZA_CDC_CMD_VERB, verb)
                            && (eResponse == RB_NO_RESPONSE_EXPECTED))
                        {
                            // NULL command, with no response expected. Move on to
                            // the next stored command. Some codecs mistakenly
                            // send a response to a NULL command, in which case
                            // we treat the response just like we would for a
                            // regular command.
                            Printf(Tee::PriLow, "NULL command(0x%08x), no response expected\n",
                                   verb);
                            i--;
                        }
                        else if (eResponse != responses[i])
                        {
                            Printf(Tee::PriError, "Sent 0x%08x, expctd 0x%08x%08x, rcvd 0x%08x%08x\n",
                                   verb,
                                   (UINT32) (eResponse >> 32),
                                   (UINT32) eResponse,
                                   upperBits,
                                   (UINT32) responses[i]);

                            if (i-1 >= 0)
                            {
                                Printf(Tee::PriHigh, "    Response[%d]: 0x%08x%08x\n",
                                       i - 1,
                                       (UINT32) (responses[i-1] >> 32),
                                       (UINT32) (responses[i-1]));
                            }
                            if (i+1 < nPopped)
                            {
                                Printf(Tee::PriHigh, "    Response[%d]: 0x%08x%08x\n",
                                        i + 1,
                                       (UINT32) (responses[i+1] >> 32),
                                       (UINT32) (responses[i+1]));
                            }

                            // For simulation timing issues cause the overrun
                            // test to fail since some responses are lost.
                            // Dont report an error on simulation if the overrun
                            // test fails due a response mismatch.
                            #ifdef SIM_BUILD
                                if (!m_IsEnableOverrunTest)
                                    m_IsError = true;
                            #else
                                m_IsError = true;
                            #endif
                        }
                        m_Status.pushedVerbs->pop();
                        m_Status.expectedResponses->pop();
                    }
                    m_Status.commandsFinished++;
                }
            }
        }
    }
    else
    {
        // Make sure that RIntFlag was correctly unasserted.
        // In lwpu's implementation there is a one-frame delay between
        // the update of the RIRB write pointer and assertion of an
        // interrupt, so occasionally it can look like RIntFlag should
        // have been set when it wasn't.
        //
        // In that one extra frame, at most N entries can arrive, with N
        // being the number of active sdin lines. Therefore, we only flag
        // an error if the number of entries is greater than or equal to
        // RIntCnt plus N.
        UINT32 N = 0;
        for (UINT32 i=0; i<MAX_NUM_SDINS; i++)
        {
            AzaliaCodec* pCodec = NULL;

            if ( (m_pAza->GetCodec(i, &pCodec) == OK) &&
                 (pCodec != NULL))
            {
                N++;
            }
        }

        if ((m_RIntCnt != 0)
            && ((m_RIntCnt + N) < nResponses))
        {
            Printf(Tee::PriError, "RIntFlag should have been set! (%d > %d + %d)\n",
                   nResponses, m_RIntCnt, N);
            return RC::AZATEST_FAIL;
        }
    }

    // Pop NULL commands from the front of the expected response list. This
    // prevents a timeout situation when the last command(s) we sent were
    // NULL commands and thus wouldn't have a response.
    while (!m_Status.expectedResponses->empty()
           && m_Status.commandsFinished < m_Status.commandsStarted)
    {
        UINT32 verb = m_Status.pushedVerbs->front();
        UINT64 eResponse = m_Status.expectedResponses->front();
        if (!RF_VAL(AZA_CDC_CMD_VERB, verb) || !(eResponse == RB_NO_RESPONSE_EXPECTED))
        {
            break;
        }
        Printf(Tee::PriDebug, "Popping NULL command (0x%08x) from front of queue.\n", verb);
        m_Status.pushedVerbs->pop();
        m_Status.expectedResponses->pop();
        m_Status.commandsFinished++;
    }

    // Check for end condition
    if (!m_IsFinished
        && ((m_Status.commandsFinished >= m_NumCommands) || m_IsError))
    {
        if (!m_IsError)
        {
            // Were we looking for a RIRB overrun?
            if (m_Status.rirbOverrunStatus == IN_PROGRESS)
            {
                Printf(Tee::PriError, "Never detected RIRB overrun when one was expected\n");
                m_Status.rirbOverrunStatus = COMPLETED_FAIL;
                return RC::AZATEST_FAIL;
            }

            // Make sure RIRB is empty
            CHECK_RC(m_pAza->RirbGetNResponses(&nResponses));
            if (nResponses > 0)
            {
                Printf(Tee::PriError, "RIRB should be empty!\n");
                m_pAza->PrintCorbState(Tee::PriNormal);
                m_pAza->PrintRirbState(Tee::PriNormal);
                vector<UINT64> responses(nResponses);
                CHECK_RC(m_pAza->RirbPop(&responses[0], nResponses, &nPopped));
                for (INT32 i=0; i<nPopped; i++)
                {
                    Printf(Tee::PriNormal, "    Unexpectedly received 0x%08x%08x\n",
                           (UINT32)(responses[i] >> 32), (UINT32) responses[i]);
                }
                return RC::AZATEST_FAIL;
            }

            UINT16 nResponses = 0;
            for (UINT08 codec = 0; codec < MAX_NUM_SDINS; ++codec)
            {
                // CHeck for unprocessed soliticited respones
                CHECK_RC( m_pAza->RirbGetNSolicitedResponses(codec, &nResponses) );
                if (nResponses > 0)
                {
                    Printf(Tee::PriHigh, "Codec %d has unprocessed solicited resposnses!\n",
                           codec);
                    m_IsError = true;
                }

                CHECK_RC( m_pAza->RirbGetNUnsolicitedResponses(codec, &nResponses) );
                if (nResponses > 0)
                {
                    Printf(Tee::PriHigh, "Codec %d has unprocessed unsolicited resposnses!\n",
                           codec);

                    UINT32 response;
                    while (nResponses > 0)
                    {
                        CHECK_RC( m_pAza->RirbGetUnsolictedResponse(codec, &response) );
                        Printf(Tee::PriHigh, "Unsolicited response 0x%08x found for codec %d\n",
                               response, codec);
                        CHECK_RC( m_pAza->RirbGetNUnsolicitedResponses(codec, &nResponses) );
                    }
                    m_IsError = true;
                }
            }
        }
        m_IsFinished = true;
    }
    return OK;
}

RC AzaliaRingBufferTestModule::SendCommands()
{
    RC rc;
    UINT16 value;

    // If we haven't already, select the next chunk size and free depth
    if (!m_Status.chunkSize)
    {
        m_Status.chunkSize = AzaliaUtilities::GetRandom(1, m_CorbSize - 1);
        if (m_Status.commandsStarted + m_Status.chunkSize > m_NumCommands)
        {
            m_Status.chunkSize = m_NumCommands - m_Status.commandsStarted;
        }
        m_Status.requiredFreeSpace = AzaliaUtilities::GetRandom(m_Status.chunkSize,
                                                                     m_CorbSize);
    }

    // Check if it's time to restart the RIRB
    if (m_Status.rirbInfo.stopped
        && Platform::GetTimeUS() > m_Status.rirbInfo.resumeTime)
    {
        Printf(Tee::PriLow, "Restarting the RIRB\n");
        CHECK_RC(ToggleRirbRunning(true));
        m_Status.rirbInfo.stopped = false;
        m_Status.rirbInfo.commandsUntilPause +=
            AzaliaUtilities::GetRandom(0, RB_MAX_COMMANDS_BEFORE_PAUSE);
    }

    // Check if it's time to restart CORB
    if (m_Status.corbInfo.stopped
        && Platform::GetTimeUS() > m_Status.corbInfo.resumeTime
        && !m_Status.rirbInfo.stopped)
    {
        Printf(Tee::PriLow, "Restarting the CORB\n");
        CHECK_RC(ToggleCorbRunning(true));
        m_Status.corbInfo.commandsUntilPause +=
            AzaliaUtilities::GetRandom(0, RB_MAX_COMMANDS_BEFORE_PAUSE);
    }

    // Push more commands to CORB
    CHECK_RC(m_pAza->CorbGetFreeSpace(&value));
    if (value >= m_Status.requiredFreeSpace
        && (m_Status.expectedResponses->size() +
            m_Status.chunkSize < m_RirbSize)
        && m_Status.chunkSize > 0
        && !m_IsError)
    {
        CHECK_RC(PushCommands());
        m_Status.commandsStarted += m_Status.chunkSize;
        m_Status.chunkSize = AzaliaUtilities::GetRandom(1, m_CorbSize - 1);
        if (m_Status.commandsStarted + m_Status.chunkSize > m_NumCommands)
        {
            m_Status.chunkSize = m_NumCommands - m_Status.commandsStarted;
        }
        m_Status.requiredFreeSpace = m_Status.chunkSize;
        //AzaliaUtilities::GetRandom(m_Status.chunkSize,m_CorbSize);
    }

    // Pause the CORB
    if (m_IsCorbStop
        && m_Status.commandsStarted > m_Status.corbInfo.commandsUntilPause
        && !m_Status.corbInfo.stopped)
    {
        Printf(Tee::PriLow, "Pausing CORB (%d > %d)\n",
               m_Status.commandsStarted,
               m_Status.corbInfo.commandsUntilPause);

        CHECK_RC(ToggleCorbRunning(false));
        m_Status.corbInfo.resumeTime = Platform::GetTimeUS() +
            (UINT64) AzaliaUtilities::GetRandom(RB_PAUSE_MIN_DUR, 4*RB_PAUSE_MIN_DUR);
    }

    // Pause the RIRB. This is safe because we're careful not to fill the
    // CORB with more commands than the RIRB can handle at any one time.
    if (m_IsRirbStop
        && !m_Status.rirbInfo.stopped
        && m_Status.corbInfo.stopped
        && m_Status.commandsStarted > m_Status.rirbInfo.commandsUntilPause
        && m_Status.commandsStarted == m_Status.commandsFinished)
    {
        // TODO: I'm not sure that this actually works. The last condition will
        // almost never be met, but when I remove it, I start getting errors
        // when the RIRB is paused. Responses to some of the commands just
        // seem to go missing. Inserting a slight delay at this point (as by
        // turning on this printf) makes the error go away. But we should
        // never reach this point unless the CORB is already stopped, so I
        // can't figure out why a delay would make a difference.
        Printf(Tee::PriHigh, "Pausing RIRB (%d > %d)\n",
               m_Status.commandsStarted,
               m_Status.rirbInfo.commandsUntilPause);

        CHECK_RC(ToggleRirbRunning(false));
        m_Status.rirbInfo.stopped = true;
        m_Status.rirbInfo.resumeTime = Platform::GetTimeUS() +
            (UINT64) AzaliaUtilities::GetRandom(RB_PAUSE_MIN_DUR, 4*RB_PAUSE_MIN_DUR);
    }

    return OK;
}
