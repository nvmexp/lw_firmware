/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "azadmae.h"
#include "azactrl.h"
#include "azafmt.h"
#include "azareg.h"
#include "azautil.h"
#include "core/include/platform.h"
#include "device/include/memtrk.h"
#include "core/include/pci.h"
#include "device/include/chipset.h"
#include "cheetah/include/tegradrf.h"
#include <math.h>

#ifdef LWCPU_FAMILY_ARM
    #include "cheetah/include/tegchip.h"
#endif

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME "AzaDmaEngine"

bool AzaliaDmaEngine::PollIsFifoReady(void* pEngine)
{
    return ((AzaliaDmaEngine * )pEngine)->IsFifoReady();
}

AzaliaDmaEngine::AzaliaDmaEngine(AzaliaController* pAza, DIR Type, UINT08 Index)
    : m_EngineType(Type),
    m_EngineIndex(Index)
{
    MASSERT(pAza);

    m_pAza = pAza;
    m_TrafficPriority = TP_LOW;
    m_StripeLines = SL_ONE;
    m_StreamDir = Type;
    m_StreamNumber = 0;
}

AzaliaDmaEngine::~AzaliaDmaEngine()
{
    // Do nothing
}

RC AzaliaDmaEngine::GetType(DIR* pType)
{
    MASSERT(pType);
    *pType = m_EngineType;
    return OK;
}

RC AzaliaDmaEngine::GetDir(DIR* pDir)
{
    MASSERT(pDir);
    *pDir = m_StreamDir;
    return OK;
}

RC AzaliaDmaEngine::GetStreamNumber(UINT08 *pIndex)
{
    MASSERT(pIndex);
    *pIndex = m_StreamNumber;
    return OK;
}

RC AzaliaDmaEngine::GetEngineIndex(UINT08 *pIndex)
{
    MASSERT(pIndex);
    *pIndex = m_EngineIndex;
    return OK;
}

RC AzaliaDmaEngine::SetStreamNumber(UINT32 StreamNumber)
{
    m_StreamNumber = StreamNumber;
    return OK;
}

RC AzaliaDmaEngine::ProgramEngine(AzaliaFormat* Format, UINT32 BufferLengthInBytes,
                                  UINT08 NumBdlEntries, void* pBdl,
                                  TRAFFIC_PRIORITY Tp, STRIPE_LINES Sl)
{
    RC rc;
    // Reset the stream to clear the registers and put us into a known state
    CHECK_RC(SetEngineState(STATE_RESET));
    CHECK_RC(SetEngineState(STATE_STOP));

    // Record the values locally
    m_TrafficPriority = Tp;
    m_StripeLines = Sl;

    // Program the SDCTL register
    UINT32 value = 0; // disable all interrupts, run=reset=0
    CHECK_RC(m_pAza->RegWr(SD_CTLX(m_EngineIndex), value));
    value =
        RF_NUM(AZA_SDCTL_STRM, m_StreamNumber) |
        RF_NUM(AZA_SDCTL_DIR, m_StreamDir) |
        RF_NUM(AZA_SDCTL_TP, m_TrafficPriority) |
        RF_NUM(AZA_SDCTL_STRIPE, m_StripeLines);
    CHECK_RC(m_pAza->RegWr(SD_CTLY(m_EngineIndex), value));

    // Clear hw and sw interrupt status
    CHECK_RC(m_pAza->IntStreamUpdateStatus(m_EngineIndex));
    CHECK_RC(m_pAza->IntStreamClearStatus(m_EngineIndex));

    // Program stream cyclic buffer length and last valid index
    CHECK_RC(m_pAza->RegWr(SD_CBL(m_EngineIndex), BufferLengthInBytes));
    CHECK_RC(m_pAza->RegWr(SD_LVI(m_EngineIndex), NumBdlEntries-1));

    // Program format
    value =
        RF_NUM(AZA_SDFMT_BASE, Format->GetRateBase()) |
        RF_NUM(AZA_SDFMT_MULT, Format->GetRateMult()) |
        RF_NUM(AZA_SDFMT_DIV, Format->GetRateDiv()) |
        RF_NUM(AZA_SDFMT_BITS, Format->GetSize()) |
        RF_NUM(AZA_SDFMT_CHAN, Format->GetChannels());

    CHECK_RC(m_pAza->RegWr(SD_FMT(m_EngineIndex), value));

    // Program BDL base address
    PHYSADDR pBdlPhys;
    if (m_pAza && m_pAza->GetIsUseMappedPhysAddr())
    {
        CHECK_RC(MemoryTracker::GetMappedPhysicalAddress(pBdl, m_pAza, &pBdlPhys));
    }
    else
    {
        pBdlPhys = Platform::VirtualToPhysical(pBdl);
    }
    CHECK_RC(m_pAza->RegWr(SD_BDPL(m_EngineIndex), pBdlPhys));
    CHECK_RC(m_pAza->RegWr(SD_BDPU(m_EngineIndex), pBdlPhys >> 32));

    // Reset DMA pos
    CHECK_RC(m_pAza->DmaPosClear(m_EngineIndex));

    return OK;
}

const char * AzaliaDmaEngine::m_StateNames[] = { "STATE_STOP", "STATE_RESET", "STATE_RUN" };

RC AzaliaDmaEngine::SetEngineState(STATE target)
{
    RC rc;
    UINT32 value;
    STATE current;
    MASSERT(target == STATE_STOP || target == STATE_RESET || target == STATE_RUN);
    MASSERT(target <= NUMELEMS(m_StateNames) - 1);

    CHECK_RC(GetEngineState(&current));
    if (current != target)
    {
        Printf(Tee::PriDebug, "[%s] Engine[%u] from %s to %s\n",
                __FUNCTION__,
                m_EngineIndex,
                m_StateNames[current],
                m_StateNames[target]);
    }
    else
    {
        return OK;
    }

    CHECK_RC(m_pAza->RegRd(SD_CTLX(m_EngineIndex), &value));
    if (current == STATE_RESET)
    {
        // Move out of reset, if necessary. We can't go directly from reset to run.
        value = FLD_SET_RF_DEF(AZA_SDCTL_SRST, _NOTRESET, value);
        CHECK_RC(m_pAza->RegWr(SD_CTLX(m_EngineIndex), value));

        AzaRegPollArgs waitArgs;
        waitArgs.pAzalia = m_pAza;
        waitArgs.reg = SD_CTLX(m_EngineIndex);
        waitArgs.mask = RF_SHIFTMASK(AZA_SDCTL_SRST);
        waitArgs.value = RF_DEF(AZA_SDCTL_SRST, _NOTRESET);
        CHECK_RC(POLLWRAP_HW(AzaRegWaitPoll, &waitArgs, Tasker::GetDefaultTimeoutMs()));
    }
    else if (current == STATE_RUN)
    {
        // Move out of run, if necessary. We can't go directly from run to reset
        value = FLD_SET_RF_DEF(AZA_SDCTL_RUN, _STOP, value);
        CHECK_RC(m_pAza->RegWr(SD_CTLX(m_EngineIndex), value));
    }

    // Current state is stopped. Move to desired state, if necessary.
    if (target == STATE_RUN)
    {
        value = FLD_SET_RF_DEF(AZA_SDCTL_RUN, _RUN, value);
        CHECK_RC(m_pAza->RegWr(SD_CTLX(m_EngineIndex), value));
    }
    else if (target == STATE_RESET)
    {
        value = FLD_SET_RF_DEF(AZA_SDCTL_SRST, _RESET, value);
        CHECK_RC(m_pAza->RegWr(SD_CTLX(m_EngineIndex), value));

        AzaRegPollArgs waitArgs;
        waitArgs.pAzalia = m_pAza;
        waitArgs.reg = SD_CTLX(m_EngineIndex);
        waitArgs.mask = RF_SHIFTMASK(AZA_SDCTL_SRST);
        waitArgs.value = RF_DEF(AZA_SDCTL_SRST, _RESET);
        CHECK_RC(POLLWRAP_HW(AzaRegWaitPoll, &waitArgs, Tasker::GetDefaultTimeoutMs()));
    }

    return OK;
}

RC AzaliaDmaEngine::GetEngineState(AzaliaDmaEngine::STATE* Current)
{
    MASSERT(Current);
    RC rc;
    UINT32 value;
    CHECK_RC(m_pAza->RegRd(SD_CTLX(m_EngineIndex), &value));
    switch (value & (RF_SHIFTMASK(AZA_SDCTL_SRST) |
                     RF_SHIFTMASK(AZA_SDCTL_RUN)))
    {
        case 0x0: *Current = STATE_STOP;  break;
        case 0x1: *Current = STATE_RESET; break;
        case 0x2: *Current = STATE_RUN;   break;
        default:
            Printf(Tee::PriError, "Unknown stream state!\n");
            return RC::SOFTWARE_ERROR;
    }
    return OK;
}

UINT32 AzaliaDmaEngine::GetPosition()
{
    RC rc;
    UINT32 value;
    rc = m_pAza->RegRd(SD_PICB(m_EngineIndex), &value);
    if (rc != OK)
    {
        Printf(Tee::PriError, "Could not read position register!\n");
        return false;
    }
    return RF_VAL(AZA_SDPICB_LPIB, value);
}

UINT32 AzaliaDmaEngine::GetPositiolwiaDma()
{
    RC rc;
    UINT32 dmaPos;

    rc = m_pAza->DmaPosGet(m_EngineIndex, &dmaPos);

    if (rc != OK)
    {
        Printf(Tee::PriError, "Could no read position via DMA!\n");
        return false;
    }
    return dmaPos;
}

bool AzaliaDmaEngine::IsFifoReady()
{
    RC rc;
    UINT32 value;
    rc = m_pAza->RegRd(SD_STS(m_EngineIndex), &value);
    if (rc != OK)
    {
        Printf(Tee::PriError, "Could not read fifo ready bit!\n");
        return false;
    }
    return FLD_TEST_RF_DEF(AZA_SDCTL_FIFORDY, _READY, value);
}

UINT16 AzaliaDmaEngine::GetFifoSize()
{
    UINT32 value;
    RC rc;
    rc = m_pAza->RegRd(SD_FIFOD(m_EngineIndex), &value);
    value = RF_VAL(AZA_SDFMT_FIFOS, value);
    return (UINT16) value;
}

RC AzaliaDmaEngine::PrintInfo(bool isDebug)
{
    constexpr auto Pri = Tee::PriDebug;
    RC rc;
    UINT32 value;
    UINT32 value2;
    UINT32 lvi;
    PHYSADDR baseAddr;

    Printf(Pri, "Stream engine %d state:\n", m_EngineIndex);
    CHECK_RC(m_pAza->RegRd(SD_CTLX(m_EngineIndex), &value));
    CHECK_RC(m_pAza->RegRd(SD_CTLY(m_EngineIndex), &value2));
    Printf(Pri, "    Stream control        = 0x%08x\n", (value2) | value);
    CHECK_RC(m_pAza->RegRd(SD_STS(m_EngineIndex), &value));
    Printf(Pri, "    Stream status         = 0x%08x\n", (value >> 24));
    CHECK_RC(m_pAza->RegRd(SD_PICB(m_EngineIndex), &value));
    Printf(Pri, "    LPIB                  = 0x%08x\n", value);
    Printf(Pri, "    DMA Position          = 0x%08x\n", GetPositiolwiaDma());
    CHECK_RC(m_pAza->RegRd(SD_CBL(m_EngineIndex), &value));
    Printf(Pri, "    CyclicBufferLength    = 0x%08x\n", value);
    CHECK_RC(m_pAza->RegRd(SD_LVI(m_EngineIndex), &lvi));
    Printf(Pri, "    LastValidIndex        = 0x%08x\n", lvi);
    CHECK_RC(m_pAza->RegRd(SD_FIFOD(m_EngineIndex), &value));
    value = RF_VAL(AZA_SDFMT_FIFOS, value);
    Printf(Pri, "    FifoSize              = 0x%08x\n", value);
    CHECK_RC(m_pAza->RegRd(SD_FMT(m_EngineIndex), &value));
    value = RF_VAL(31:16, value);
    Printf(Pri, "    Format                = 0x%08x\n", value);
    CHECK_RC(m_pAza->RegRd(SD_BDPL(m_EngineIndex), &value));
    Printf(Pri, "    LowerBase             = 0x%08x\n", value);
    CHECK_RC(m_pAza->RegRd(SD_BDPU(m_EngineIndex), &value2));
    Printf(Pri, "    UpperBase             = 0x%08x\n", value2);
    baseAddr = (static_cast<PHYSADDR>(value2) << 32) | value;
    if (isDebug)
    {
        Printf(Pri, "    BDL:\n");
        const UINT16 bdleSize = 16;
        for (UINT32 i=0; i<lvi; i++)
        {
            void *vaddr = NULL;
            vaddr = MemoryTracker::PhysicalToVirtual(baseAddr + (bdleSize * i) + 0x0);
            UINT32 addr = MEM_RD32(vaddr);

            vaddr = MemoryTracker::PhysicalToVirtual(baseAddr + (bdleSize * i) + 0x8);
            UINT32 len  = MEM_RD32(vaddr);

            vaddr = MemoryTracker::PhysicalToVirtual(baseAddr + (bdleSize * i) + 0xc);
            bool ioc = MEM_RD32(vaddr) & 0x1;
            Printf(Pri, "        Entry %03d: a=0x%08x, l=%08x, ioc=%d\n",
                   i, addr, len, ioc);
        }
    }
    return OK;
}

// A01 parts don't have ECO present
bool AzaliaDmaEngine::IsEcoPresent()
{
    RC rc = OK;
    UINT32 data=0;
    UINT32 domain,bus,dev,func;

    CHECK_RC(Platform::FindPciClassCode(CLASS_CODE_LPC,
                                        0,
                                        &domain,
                                        &bus,
                                        &dev,
                                        &func));
    CHECK_RC(Platform::PciRead32(domain,bus,dev,func,0x08,&data));
    UINT08 revId = RF_VAL(7:0,data);
    return (revId != 0xA1) ? true : false;
}

UINT16 AzaliaDmaEngine::ExpectedFifoSize(AzaliaDmaEngine::DIR dir,
                                         AzaliaFormat* pformat)
{
    UINT16 expectedFifoSize = 0;
    const UINT32 BytesPerPacket =
        pformat->GetSizeInBytes() *
        pformat->GetChannelsAsInt() *
        pformat->GetRateMultAsInt();

    // Computation of GPU Fifo Size provided by Ka Yun Lee
    if (m_pAza->IsGpuAza())
    {
        if (dir == DIR_OUTPUT)
        {
            switch (BytesPerPacket)
            {
                case 1:  expectedFifoSize = 40;  break;
                case 2:  expectedFifoSize = 88;  break;
                case 3:  expectedFifoSize = 128; break;
                case 4:  expectedFifoSize = 176; break;
                case 5:  expectedFifoSize = 216; break;
                case 6:  expectedFifoSize = 264; break;
                case 7:  expectedFifoSize = 304; break;
                case 8:  expectedFifoSize = 352; break;
                case 10: expectedFifoSize = 440; break;
                case 13: expectedFifoSize = 560; break;
                case 17: expectedFifoSize = 744; break;
                default: expectedFifoSize = ((UINT32)floor(BytesPerPacket*44.0/64.0)) * 64;
            }

            // Bug 733199: Add 16 to the callwlated FIFO size since the
            // FIFO size was changed for GF119 azalia to be 16 more.
            UINT16 devId = 0;
            m_pAza->CfgRd16(LW_PCI_DEVICE_ID, &devId);

            expectedFifoSize += 16;
        }
        return expectedFifoSize;
    }

    UINT16 devId = 0;
    m_pAza->CfgRd16(LW_PCI_DEVICE_ID, &devId);

    if (m_pAza->IsIch6()) // Intel ICH6 HDA
    {
        AzaliaFormat::SIZE size = pformat->GetSize();
        if (dir == DIR_OUTPUT)
        {
            if (size == AzaliaFormat::SIZE_20 ||
                size == AzaliaFormat::SIZE_24)
                expectedFifoSize = 0xFF; // default value
            else
                expectedFifoSize = 0xBF; // default value
        }
        else
        {
            if (size == AzaliaFormat::SIZE_20 ||
                size == AzaliaFormat::SIZE_24)
                expectedFifoSize = 0x9F; // default value
            else
                expectedFifoSize = 0x77; // default value
        }
    }
    else // Lwpu Chips
    {
        if (dir == DIR_OUTPUT)
        {
        #ifdef LWCPU_FAMILY_ARM
            UINT32 data = 0;
            UINT32 rawFifoSize = 0;
            UINT32 maxFifoSize = 0;
            UINT32 numFrames2Ch = 0;
            UINT32 numFrames6Ch = 0;
            UINT32 numFrames8Ch = 0;

            m_pAza->CfgRd32(OB_BUFSZ_NUM_FRAMES, &data);
            numFrames2Ch = RF_VAL(AZA_OB_BUFSZ_NUM_OF_FRAMES_2CHAN, data);
            numFrames6Ch = RF_VAL(AZA_OB_BUFSZ_NUM_OF_FRAMES_6CHAN, data);
            numFrames8Ch = RF_VAL(AZA_OB_BUFSZ_NUM_OF_FRAMES_8CHAN, data);

            if (pformat->GetChannelsAsInt() <= 2)
                rawFifoSize = BytesPerPacket * numFrames2Ch;
            else if (pformat->GetChannelsAsInt() <= 6)
                rawFifoSize = BytesPerPacket * numFrames6Ch;
            else
                rawFifoSize = BytesPerPacket * numFrames8Ch;

            maxFifoSize = 128 * AZA_OB_BUFSZ_NUM_OF_FRAMES_8CHAN_MAX;

            if(IsEcoPresent())
            {
                maxFifoSize += 16;
                rawFifoSize += 16;
            }

            if      (rawFifoSize < 16)          { expectedFifoSize = 16; }
            else if (rawFifoSize > maxFifoSize) { expectedFifoSize = maxFifoSize; }
            else                                { expectedFifoSize = rawFifoSize; }
        #else
            // This algorithm, provided by Govendra Gupta, is for
            // callwlating expected frame sizes for chips with system
            // stutter defined (which started in MCP77).
            // We use this algorighm for all non-LWPU chipsets...
            UINT32 pages_reqd = 15;
            if (BytesPerPacket < 51)
            {
                pages_reqd = (BytesPerPacket * 20) >> 6;
            }
            if      (BytesPerPacket <= 1) { expectedFifoSize =  32; }
            else if (BytesPerPacket <= 3) { expectedFifoSize =  64; }
            else if (BytesPerPacket <= 6) { expectedFifoSize = 128; }
            else
            {
                if (pages_reqd >= 57) { expectedFifoSize = 3712; }
                else expectedFifoSize = 128 + ((pages_reqd - 1) * 64);
            }
        #endif
        }
        else
        {
            // Input fifo sizing rules for Lwpu implementation
            if      (BytesPerPacket <= 4) expectedFifoSize = 32;
            else if (BytesPerPacket <= 8) expectedFifoSize = 64;
            else if (BytesPerPacket <=16) expectedFifoSize = 128;
            else                          expectedFifoSize = 256;
        }
    }

    Printf(Tee::PriDebug, "BytesPerPacket = %d (%d * %d * %d), fifosize %d\n",
           BytesPerPacket,
           pformat->GetSizeInBytes(),
           pformat->GetChannelsAsInt(),
           pformat->GetRateMultAsInt(),
           expectedFifoSize);

    return(expectedFifoSize);
}

