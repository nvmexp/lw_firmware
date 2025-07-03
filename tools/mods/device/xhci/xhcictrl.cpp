/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "xhcictrl.h"
#include "xhcictrlmgr.h"
#include "core/include/tee.h"
#include "core/include/rc.h"
#include "core/include/platform.h"
#include "cheetah/include/logmacros.h"
#include "cheetah/include/tegradrf.h"
#include "device/include/memtrk.h"
#include "core/include/xp.h"
#include "core/include/data.h"
#include "core/include/utility.h"
#include "cheetah/dev/clk/clkdev.h"
#ifdef TEGRA_MODS
    #include "cheetah/include/sysutil.h"
#endif
#include <algorithm>

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME "XhciController"

#define XHCI_FW_IMEM_BLOCK_SIZE     256
#define XHCI_FW_CFG_TBL_SIZE        256

#define XHCI_REG_RANGE              0x10000//64K so far

CONTROLLER_NAMED_ISR(0, XhciController, Common, Isr)

bool XhciController::m_IsFullCopy       = false;

XhciController::XhciController()
{
    LOG_ENT();

    m_vUserContext.clear();
    m_UserContextAlign  = 0;
    m_vUserStreamLayout.clear();

    m_NewSlotId         = 0;
    m_CapLength         = 0;
    m_DoorBellOffset    = 0;
    m_RuntimeOffset     = 0;

    m_HcVer             = 0;

    m_MaxSlots          = 0;
    m_MaxIntrs          = 0;
    m_MaxPorts          = 0;

    m_MaxEventRingSize  = 0;
    m_MaxSratchpadBufs  = 0;

    m_ExtCapPtr         = 0;
    m_IsNss             = false;
    m_MaxPSASize        = 0;
    m_vPortProtocol.clear();

    m_PageSize          = 0;

    m_pScratchpadBufferArray    = nullptr;
    m_pDevContextArray  = nullptr;

    m_UuidOffset        = 0;
    m_pCmdRing          = nullptr;

    for ( UINT32 i = 0; i <= XHCI_MAX_SLOT_NUM; i++ )
    {
        m_pUsbDevExt[i]  = nullptr;
    }

    for ( UINT32 i = 0; i < XHCI_MAX_INTR_NUM; i++ )
    {
        m_pEventRings[i] = nullptr;
        m_EventCount[i]  = 0;
        m_Event[i] = nullptr;
    }

    m_vpRings.clear();
    m_vpStreamArray.clear();

    m_CmdRingSize       = 13;
    m_EventRingSize     = 15;
    m_XferRingSize      = 17;

    m_IsInsertEventData = false;
    m_pDfiMem           = nullptr;
    m_DfiLength         = 0;
    m_pDfiScratchMem    = nullptr;

    m_pRegBuffer        = nullptr;
    m_IsFromLocal       = false;
    m_vRunningEp.clear();

    m_Ratio64Bit        = 100;
    m_RatioIdt          = 0;
    m_EventData         = 0;

    m_pDummyBuffer      = nullptr;
    m_DummyBufferSize   = 0;

    m_OffsetPciMailbox  = 0;
    m_CErr              = 3;

    m_PendingDmaCmd     = 0;
    m_DmaCount          = 0;

    m_EnumMode          = 0;
    m_TestModePortIndex = 0;

    m_pLogBuffer        = nullptr;
    m_pFwLogDeqPtr      = nullptr;

    m_SSPortMap         = 0x0;
    m_LeadingSpace      = 0x0;

    m_IsSkipRegWrInInit = false;

    m_PhysBAR2          = 0;
    m_pBar2             = nullptr;

    m_IsGetSetCap       = false;
    m_Eci               = EXTND_CAP_ID_ILWALID;
    m_IsBto             = false;
    m_BusErrCount       = 0;
    LOG_EXT();
}

XhciController::~XhciController()
{
    LOG_ENT();

    if ( m_pDfiMem && MemoryTracker::IsValidVirtualAddress(m_pDfiMem) )
    {
        if ( m_pDfiScratchMem && MemoryTracker::IsValidVirtualAddress(m_pDfiScratchMem) )
        {
            MemoryTracker::FreeBuffer(m_pDfiScratchMem);
            m_pDfiScratchMem = nullptr;
        }
        if ( m_pLogBuffer )
        {
            MemoryTracker::FreeBuffer(m_pLogBuffer);
            m_pLogBuffer = nullptr;
        }
        MemoryTracker::FreeBuffer(m_pDfiMem);
        m_pDfiMem = nullptr;
    }

    // user inpur variables
    m_vUserStreamLayout.clear();
    m_vUserContext.clear();

    LOG_EXT();
}

const char* XhciController::GetName()
{
    return CLASS_NAME;
}

RC XhciController::ReleaseDataStructures()
{
    LOG_ENT();
    // this to be cleared list is grab from constructor
    // including vectors, arrays and anything that shoulc be cleared after user call Xhci.Uninit()
    m_vPortProtocol.clear();
    m_vRunningEp.clear();

    for ( UINT32 i = 0; i < XHCI_MAX_INTR_NUM; i++ )
    {   // delete all the Event objects
        if ( m_Event[i] )
        {
            Tasker::FreeEvent(m_Event[i]);
            m_Event[i] = NULL;
        }
        if ( m_pEventRings[i] )
        {   // Ring 0 will be released in UninitDataStructures()
            delete m_pEventRings[i];
            m_pEventRings[i] = NULL;
        }
        m_EventCount[i]  = 0;
    }

    for ( UINT32 i = 0; i <= XHCI_MAX_SLOT_NUM; i++ )
    {   // delete all the UsbDevExt objects
        if ( m_pUsbDevExt[i] )
        {
            delete m_pUsbDevExt[i];
            m_pUsbDevExt[i] = NULL;
        }
    }

    for ( UINT32 i = 0; i < m_vpRings.size(); i++ )
    {   // delete all the Rings
        if ( m_vpRings[i] )
        {
            delete m_vpRings[i];
            m_vpRings[i] = NULL;
        }
    }
    m_vpRings.clear();

    for ( UINT32 i = 0; i < m_vpStreamArray.size(); i++ )
    {   // delete all the Rings
        if ( m_vpStreamArray[i] )
        {
            delete m_vpStreamArray[i];
            m_vpStreamArray[i] = NULL;
        }
    }
    m_vpStreamArray.clear();
    LOG_EXT();
    return OK;
}

RC XhciController::SetEventData(bool IsEnable)
{
    Printf(Tee::PriNormal, "Event Data %s \n", IsEnable?"On":"Off");
    m_IsInsertEventData = IsEnable;
    return OK;
}

RC XhciController::SetFullCopy(bool IsEnable)
{
    Printf(Tee::PriNormal, "Full Cody from user Input Context %s \n", IsEnable?"On":"Off");
    m_IsFullCopy = IsEnable;
    return OK;
}

RC XhciController::SetRnd(UINT32 Index, UINT32 Ratio)
{
    if (Index < XHCI_RATIO_64BIT || Index > XHCI_RATIO_EVENT_DATA)
    {
        Printf(Tee::PriError, "Invalid Random Index %d, valie [%d-%d] \n", Index, XHCI_RATIO_64BIT, XHCI_RATIO_EVENT_DATA);
        return RC::BAD_PARAMETER;
    }
    if (Ratio > 100)
    {
        Printf(Tee::PriError, "Bad Ratio %d, valie [0-100] \n", Ratio);
        return RC::BAD_PARAMETER;
    }
    switch(Index)
    {
        case XHCI_RATIO_64BIT:
            Printf(Tee::PriNormal, "64Bit address ratio %d%% ", m_Ratio64Bit);
            m_Ratio64Bit = Ratio;
            Printf(Tee::PriNormal, "-> %d%%\n ", m_Ratio64Bit);
            break;
        case XHCI_RATIO_IDT:
            Printf(Tee::PriNormal, "IDT ratio %d%% ", m_RatioIdt);
            m_RatioIdt = Ratio;
            Printf(Tee::PriNormal, "-> %d%%\n ", m_RatioIdt);
            break;
        case XHCI_RATIO_EVENT_DATA:
            Printf(Tee::PriNormal, "EventData ratio %d%% ", m_EventData);
            m_EventData = Ratio;
            Printf(Tee::PriNormal, "-> %d%%\n ", m_EventData);
            break;
    }
    return OK;
}

RC XhciController::GetPageSize(UINT32 * pPageSize)
{
    if ( m_InitState < INIT_REG )
    {
        Printf(Tee::PriError, "PageSize not init \n");
        return RC::WAS_NOT_INITIALIZED;
    }
    *pPageSize = m_PageSize;
    return OK;
}

RC XhciController::RegRd(const UINT32 Offset, UINT32 * pData, const bool IsDebug) const
{
    RC rc;
    UINT32 mutableOffset = Offset;
    if ( mutableOffset & XHCI_REG_TYPE_OPERATIONAL )
    {
        mutableOffset &= ~XHCI_REG_TYPE_OPERATIONAL;
        mutableOffset += m_CapLength;
    }
    else if ( mutableOffset & XHCI_REG_TYPE_DOORBELL )
    {
        mutableOffset &= ~XHCI_REG_TYPE_DOORBELL;
        mutableOffset += m_DoorBellOffset;
    }
    else if ( mutableOffset & XHCI_REG_TYPE_RUNTIME )
    {
        mutableOffset &= ~XHCI_REG_TYPE_RUNTIME;
        mutableOffset += m_RuntimeOffset;
    }
    if ( mutableOffset & 0x3 )
    {
        Printf(Tee::PriHigh, "Rd Offset un-aligned: 0x%x \n",  mutableOffset);
        PrintState();
        return RC::SOFTWARE_ERROR;
    }

    if ( pData )
    {
        if ( !m_IsFromLocal )
        {
            CHECK_RC(Controller::RegRd(mutableOffset, pData));
        }
        else
        {
            *pData = MEM_RD32(((char *)m_pRegBuffer + mutableOffset));
        }
    }
    return OK;
}

RC XhciController::RegWr(const UINT32 Offset, const UINT32 Data, const bool IsDebug) const
{
    RC rc;
    UINT32 mutableOffset = Offset;
    if ( mutableOffset & XHCI_REG_TYPE_OPERATIONAL )
    {
        mutableOffset &= ~XHCI_REG_TYPE_OPERATIONAL;
        mutableOffset += m_CapLength;
    }
    else if ( mutableOffset & XHCI_REG_TYPE_DOORBELL )
    {
        mutableOffset &= ~XHCI_REG_TYPE_DOORBELL;
        mutableOffset += m_DoorBellOffset;
    }
    else if ( mutableOffset & XHCI_REG_TYPE_RUNTIME )
    {
        mutableOffset &= ~XHCI_REG_TYPE_RUNTIME;
        mutableOffset += m_RuntimeOffset;
    }
    if ( mutableOffset & 0x3 )
    {
        Printf(Tee::PriHigh, " Wr Offset un-aligned: 0x%x",  Offset);
        PrintState();
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(Controller::RegWr(mutableOffset, Data));

    return OK;
}

RC XhciController::SetPciReservedSize()
{
    if (GetInitState() >= INIT_BAR)
    {
        Printf(Tee::PriError, "SetPciReservedSize() should be called before BAR init - Init(0) \n");
        return RC::WAS_NOT_INITIALIZED;
    }
    m_LeadingSpace = 0x10000;
    return OK;
}

RC XhciController::InitBar()
{
    LOG_ENT();
    RC rc = OK;

    if (m_pRegisterBase == nullptr)
    {
        Printf(Tee::PriLow, "InitBar(): mapping BAR @ 0x%llx (MMIO offset 0x%x)\n",
               m_BasicInfo.RegisterPhysBase, m_LeadingSpace);
        CHECK_RC(Platform::MapDeviceMemory(&m_pRegisterBase, m_BasicInfo.RegisterPhysBase,
                                           m_BasicInfo.RegisterMapSize, Memory::UC,
                                           Memory::ReadWrite));
    }
    m_pRegisterBase = static_cast<UINT08*>(m_pRegisterBase) + m_LeadingSpace;       // abjust the BAR virtual address to correct offset

    m_CapLength      = 0xff & MEM_RD32((void*)((UINT08*)m_pRegisterBase + XHCI_REG_CAPLENGTH)); // Get the cap register length
    m_DoorBellOffset = MEM_RD32((void*)((UINT08*)m_pRegisterBase + XHCI_REG_DBOFF));     // Get the doorbell offset
    m_DoorBellOffset &= ~0x3;   // see also spec 5.3.7 Doorbell Offset (DBOFF)
    m_RuntimeOffset  = MEM_RD32((void*)((UINT08*)m_pRegisterBase + XHCI_REG_RTSOFF));    // Get Runtime Reg space offset
    m_RuntimeOffset  &= ~0x1f;   // see also spec 5.3.8 Runtime Register Space Offset (RTSOFF)

    if (m_PhysBAR2)
    {
        CHECK_RC(Platform::MapDeviceMemory(&m_pBar2, m_PhysBAR2,
                                           XHCI_BAR2_MMIO_ADDRESS_RANGE, Memory::UC,
                                           Memory::ReadWrite));
    }
    LOG_EXT();
    return rc;
}

RC XhciController::UninitBar()
{
    LOG_ENT();
    if (m_pBar2)
    {
        Platform::UnMapDeviceMemory(m_pBar2);
        m_pBar2 = nullptr;
    }

    if (m_pRegisterBase)
    {
        Platform::UnMapDeviceMemory(static_cast<UINT08*>(m_pRegisterBase) - m_LeadingSpace);
        m_pRegisterBase = nullptr;
    }
    LOG_EXT();
    return OK;
}

RC XhciController::InitRegisters()
{
    LOG_ENT();
    RC rc = OK;

    UINT32 val;
    CHECK_RC(RegRd(XHCI_REG_CAPLENGTH, &val));
    m_HcVer = val>>16;      // 0x0096 -> version 0.96
    if ( (val>>24) > 0x2 )
    {
        Printf(Tee::PriError, "Invalid Xhci Controller ver %x.%02x, Firmware error? \n", val>>24, ( val>>16 ) & 0xff);
        return RC::HW_STATUS_ERROR;
    }
    Printf(Tee::PriNormal, "Xhci Controller ver %x.%02x \n", val>>24, ( val>>16 ) & 0xff);

    // After Chip Hardware Reset3 wait until the Controller Not Ready (CNR) flag in
    // the USBSTS is '0' before writing any xHC Operational or Runtime registers.
    if ( !(XHCI_DEBUG_MODE_NO_WAIT_CNR & GetDebugMode()) )
    {   // user could skip the CNR checking by calling Xhci.SetDebugMode(true) before Xhci.Init()
#ifdef SIM_BUILD
        Printf(Tee::PriNormal, "Start to wait for CNR bit clear... ");
        UINT32 usbSts;
        RegRd(XHCI_REG_OPR_USBSTS, & usbSts);
        while(FLD_TEST_RF_NUM(XHCI_REG_OPR_USBSTS_CNR, 0x1, usbSts))
        {   // infinite looop until CNR is cleared by HW
            RegRd(XHCI_REG_OPR_USBSTS, & usbSts);
        }
        Printf(Tee::PriNormal, "exit \n");
#else
        rc = WaitRegMem(m_CapLength+(XHCI_REG_OPR_USBSTS^XHCI_REG_TYPE_OPERATIONAL), 0, RF_SHIFTMASK(XHCI_REG_OPR_USBSTS_CNR), GetTimeout());
        if ( OK != rc )
        {
            Printf(Tee::PriError, "Wait for CNR timeout \n"); return rc;
        }
#endif
    }

    CHECK_RC( RegRd(XHCI_REG_HCSPARAMS1, &val) );
    m_MaxSlots = RF_VAL(XHCI_REG_HCSPARAMS1_MAXSLOTS, val);
    m_MaxIntrs = RF_VAL(XHCI_REG_HCSPARAMS1_MAXINTRS, val);
    if ( m_MaxIntrs > XHCI_MAX_INTR_NUM )
    {
        Printf(Tee::PriError, "m_MaxIntrs overflow, enlarge the XHCI_MAX_INTR_NUM \n");
        return RC::SOFTWARE_ERROR;
    }
    m_MaxPorts = RF_VAL(XHCI_REG_HCSPARAMS1_MAXPORTS, val);

    CHECK_RC( RegRd(XHCI_REG_HCSPARAMS2, &val) );
    m_MaxEventRingSize = RF_VAL(XHCI_REG_HCSPARAMS2_ERSTMAX, val);
    m_MaxSratchpadBufs = RF_VAL(XHCI_REG_HCSPARAMS2_MAXSCRATCHPADBUFS_HI, val);
    m_MaxSratchpadBufs <<= (1?XHCI_REG_HCSPARAMS2_MAXSCRATCHPADBUFS_LO) - (0?XHCI_REG_HCSPARAMS2_MAXSCRATCHPADBUFS_LO) + 1;
    m_MaxSratchpadBufs |= RF_VAL(XHCI_REG_HCSPARAMS2_MAXSCRATCHPADBUFS_LO, val);
    if ( m_MaxSratchpadBufs )
    {
        Printf(Tee::PriLow, "Host request %d pages as Scratchpad Buffers \n", m_MaxSratchpadBufs);
    }

    CHECK_RC( RegRd(XHCI_REG_HCCPARAMS, &val) );
    m_ExtCapPtr = RF_VAL(XHCI_REG_HCCPARAMS_XEPC, val);
    m_Is64ByteContext = (RF_VAL(XHCI_REG_HCCPARAMS_CSZ, val)!= 0);
    m_IsNss = (RF_VAL(XHCI_REG_HCCPARAMS_NSS, val) != 0);
    m_MaxPSASize = RF_VAL(XHCI_REG_HCCPARAMS_MAXPSASIZE, val);

    CHECK_RC( RegRd(XHCI_REG_HCCPARAMS2, &val) );
    m_IsGetSetCap = (RF_VAL(XHCI_REG_HCCPARAMS2_GSC, val) != 0);

    CHECK_RC( RegRd(XHCI_REG_OPR_PAGESIZE, &val) );
    m_PageSize = (1<<12) * RF_VAL(XHCI_REG_OPR_PAGESIZE_PS, val);

    // see InitDataStructures() for Device Context Base Array
    // see InitDataStructures() for Command Ring
    // see InitDataStructures() for Event Ring
    ReadPortProtocol();

    if (m_IsSkipRegWrInInit)
    {
        Printf(Tee::PriNormal, "InitRegisters(): skip regsiter write and exit...\n");
        return OK;
    }

    // below implement xHCI spec section 4.2
    CHECK_RC( RegRd(XHCI_REG_OPR_CONFIG, &val) );
    val = FLD_SET_RF_NUM(XHCI_REG_OPR_CONFIG_MAXSLOTEN, m_MaxSlots, val); // enable all possible slots
    CHECK_RC( RegWr(XHCI_REG_OPR_CONFIG, val) );

    // by default disable all possible Event Ring expect for Primary Event Ring
    for ( UINT32 i = 1; i <m_MaxIntrs; i++ )
    {
        CHECK_RC( RegWr(XHCI_REG_ERSTSZ(i), 0) );
    }

    // try to wait for MSG from HW
    // if there's any message inthe mailbox, handle it
    if ( !(XHCI_DEBUG_MODE_SKIP_MAILBOX & GetDebugMode()) )
    {
        HandleFwMsg(100);
    }
    LOG_EXT();
    return rc;
}

RC XhciController::InitDataStructures()
{
    LOG_ENT();
    RC rc = OK;
    // turn off controller for safe
    CHECK_RC(StartStop(false));

    // Init Device Context Base Array ------------------------------------------
    m_pDevContextArray = new DeviceContextArray();
    UINT32 addrBit;
    CHECK_RC(GetRndAddrBits(&addrBit));
    CHECK_RC(m_pDevContextArray->Init(addrBit));

    PHYSADDR pPhyAddr;
    CHECK_RC(m_pDevContextArray->GetBaseAddr(&pPhyAddr));
    Printf(Tee::PriDebug, "Dev Context Base Address Array: 0x%llx (Phys) \n",pPhyAddr);

    CHECK_RC( RegWr(XHCI_REG_OPR_DCBAAP_LO, pPhyAddr) );
    pPhyAddr = pPhyAddr >> 32;
    CHECK_RC( RegWr(XHCI_REG_OPR_DCBAAP_HI, pPhyAddr) );

    // setup the Primary Event Ring --------------------------------------------
    CHECK_RC(InitEventRing(0));

    // On my NEC USB3 card. Enable Slot command timeout(No commmand completion event) without this
    //  delay. this happens only on reentering. after reboot, things work fine even without this delay
    Tasker::Sleep(10);

    CHECK_RC(InitCmdRing());

    // allocate Scratchpad Buffer(s) if HW requires
    if ( m_MaxSratchpadBufs )
    {
        CHECK_RC(HandleScratchBuf(true));
    }

    // set the Run bit
    // make sure this sticks to the end of InitDataStructures()
    CHECK_RC(StartStop(true));

    LOG_EXT();
    return OK;
}

RC XhciController::SetIntrTarget(UINT08 SlotId, UINT08 EpNum, bool IsDataOut, UINT32 IntrTarget)
{
    RC rc;
    UINT32 ringUuid;
    CHECK_RC(FindXferRing(SlotId,EpNum,IsDataOut,&ringUuid));
    CHECK_RC(SetIntrTarget(ringUuid, IntrTarget));
    return OK;
}

RC XhciController::SetIntrTarget(UINT32 RingUuid, UINT32 IntrTarget)
{
    RC rc;
    if ( IntrTarget >= m_MaxIntrs )
    {
        Printf(Tee::PriError, "Invalid Interrupt Target, valid[0-%u] \n", m_MaxIntrs - 1);
        return RC::BAD_PARAMETER;
    }
    XhciTrbRing * pRing;
    CHECK_RC(FindRing(RingUuid, &pRing));
    if ( !pRing->IsXferRing() )
    {
        Printf(Tee::PriError, "Ring %u is not Xfer Ring \n", RingUuid);
        return RC::ILWALID_INPUT;
    }
    TransferRing *pXferRing = (TransferRing*) pRing;
    bool isEmpty;
    CHECK_RC(pXferRing->IsEmpty(&isEmpty));
    if ( !isEmpty )
    {
        Printf(Tee::PriError, "Interrupt Target could ONLY be changed when Xfer Ring is empty so far \n");
        return RC::ILWALID_INPUT;
    }
    // check if Target Event Ring exists, if not create one
    if ( m_pEventRings[IntrTarget] == NULL )
    {
        CHECK_RC(InitEventRing(IntrTarget));
    }

    CHECK_RC(pXferRing->SetInterruptTarget(IntrTarget));
    return OK;
}

RC XhciController::GetIntrTarget(UINT08 SlotId, UINT08 EpNum, bool IsDataOut, UINT32* pIntrTarget)
{
    RC rc;
    UINT32 ringUuid;
    CHECK_RC(FindXferRing(SlotId,EpNum,IsDataOut,&ringUuid));
    CHECK_RC(GetIntrTarget(ringUuid, pIntrTarget));
    return OK;
}

RC XhciController::GetIntrTarget(UINT32 RingUuid, UINT32* pIntrTarget)
{
    RC rc;
    if ( !pIntrTarget )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }

    XhciTrbRing * pRing;
    CHECK_RC(FindRing(RingUuid, &pRing));
    if ( !pRing->IsXferRing() )
    {
        Printf(Tee::PriError, "Ring %u is not Xfer Ring \n", RingUuid);
        return RC::ILWALID_INPUT;
    }
    TransferRing *pXferRing = (TransferRing*) pRing;
    CHECK_RC(pXferRing->GetInterruptTarget(pIntrTarget));
    return OK;
}

RC XhciController::CleanupTd(UINT08 SlotId, UINT08 Endpoint, bool IsDataOut, bool IsIssueStop)
{   // call Set Deq Pointer to set Deq to Enq, so we have a empty Ring
    // all HC cached TRBs will be Ilwalidated
    RC rc;

    TransferRing * pXferRing;
    CHECK_RC( FindXferRing(SlotId, Endpoint, IsDataOut, & pXferRing) );
    bool isEmpty;
    CHECK_RC(pXferRing->IsEmpty(&isEmpty));
    if ( isEmpty )
    {   // Ring is already empty, do nothings
        // return OK;  // when HC STALL a TRB, it won't advance the Deq, SW need to enforece that, don't return OK here
    }

    UINT08 dci;
    DeviceContext::EpToDci(Endpoint, IsDataOut, &dci);
    if ( IsIssueStop )
    {
        CHECK_RC(CmdStopEndpoint(SlotId, dci, false));      // stop all un-finished transfer
    }

    // get the physical address of Enq Ptr
    PHYSADDR pPa;
    bool IsDcs;
    XhciTrb *pTrb;
    CHECK_RC(pXferRing->GetEnqInfo(&pPa, &IsDcs, &pTrb));

    CHECK_RC(CmdSetTRDeqPtr(SlotId, dci, pPa, IsDcs, 0, 0));
    CHECK_RC(pXferRing->UpdateDeqPtr(pTrb,  false));
    return OK;
}

RC XhciController::PrintFwStatus()
{
    UINT32 val;

    CsbWr(0x1c0, 0x2c);
    CsbRd(0x1c4, &val);
    Printf(Tee::PriNormal, "FW Time Stamp: 0x%x\n", val);

    CsbRd(0x100, &val);
    Printf(Tee::PriNormal, "CPUCTL: 0x%x, ", val);

    CsbWr(0x200, 0x1c08);
    CsbRd(0x20c, &val);
    Printf(Tee::PriNormal, "Assertion: 0x%x\n", val);

    CsbWr(0x200,0x1508);
    CsbRd(0x20c, &val);
    Printf(Tee::PriNormal, "PC: 0x%x ", val);
    Tasker::Sleep(10);
    CsbWr(0x200,0x1508);
    CsbRd(0x20c, &val);
    Printf(Tee::PriNormal, "...re-read PC: 0x%x\n", val);

    return OK;
}

RC XhciController::LoadFW(const string FileName, bool IsEnMailbox)
{
    // see also bug 581941 for detail. ref to //sw/dev/mcp_dev/usb3/fw_util/km.c
    // see also bug http://lwbugs/747518 for new scheme
    LOG_ENT();
    RC rc;

    CHECK_RC(FindMailBoxCap());
    // enable Mailbox messaging
    if ( IsEnMailbox )
    {
        CHECK_RC(MailboxRegWr32(m_OffsetPciMailbox + USB3_PCI_OFFSET_MAILBOX_CMD, (1<<31)));
        Printf(Tee::PriLow,
               "LoadFw() with mailbox enabled - Bit31 set in PCI Cfg Reg %d \n",
               m_OffsetPciMailbox + USB3_PCI_OFFSET_MAILBOX_CMD);
    }

    if ( XHCI_DEBUG_MODE_WRITE_FW & GetDebugMode() )
    {
        Printf(Tee::PriNormal, "Skipping file loading, go to Falcon launching \n");
    }
    else
    {
        if ( m_pDfiMem )
        {   // allow loading multiple times
            // de-alloc extra mem first
            if ( m_pDfiScratchMem )
            {
                MemoryTracker::FreeBuffer(m_pDfiScratchMem);
                m_pDfiScratchMem = NULL;
            }
            MemoryTracker::FreeBuffer(m_pDfiMem);
            m_pDfiMem = NULL;
        }

        // read the FW into memory buffer
        FILE *pFile;
        CHECK_RC(Utility::OpenFile(FileName, &pFile, "rb"));
        fseek (pFile , 0 , SEEK_END);
        m_DfiLength = ftell (pFile);
        Printf(Tee::PriLow, "Firmware: %s, size %u Bytes\n", FileName.c_str(), m_DfiLength);
        //rewind (pFile);
        fseek (pFile, 0, SEEK_SET);

        UINT32 lwrrPos = ftell(pFile);
        Printf(Tee::PriLow, "File at beginning %u Bytes\n", lwrrPos);

        UINT32 allocSize = m_DfiLength;
        if ( allocSize % 4 )
        {
            Printf(Tee::PriWarn, "File size %u is not DWord aligned", allocSize);
            allocSize += 4 - (allocSize % 4);
        }

        CHECK_RC( MemoryTracker::AllocBufferAligned(allocSize, &m_pDfiMem, 256, 32, Memory::UC) );
        Memory::Fill32(m_pDfiMem, 0, allocSize / 4);

    //#ifdef SIM_BUILD
    #if defined(SIM_BUILD) || defined(INCLUDE_PEATRANS)
            // Memory allocated using Platform::Allocxxx cannot be used by
            // regular C functions the simulatione environment
            // Allocate a temporary buffer to read in the file and then
            // copy it over the buffer allocated by Memory Tracker.
            /*
            void *tmpBuff = malloc(allocSize);
            memset(tmpBuff, 0, allocSize);
            UINT32 tmp = fread(tmpBuff, 1, allocSize, pFile);
            Platform::VirtualWr(pData32, tmpBuff, allocSize);
            free(tmpBuff);
            */
            vector<UINT08> tmpBuff (allocSize, 0);
            UINT32 tmp = fread(&(tmpBuff[0]), 1, allocSize, pFile);
            Memory::Fill(m_pDfiMem, &tmpBuff);

    #else
            UINT32 tmp = fread (m_pDfiMem,1,m_DfiLength,pFile);
    #endif

        if ( m_DfiLength !=  tmp )
        {
            Printf(Tee::PriError, "File reading error, expect %u, get %u \n", m_DfiLength, tmp);
            return RC::ILWALID_INPUT;
        }
        else
        {
            Printf(Tee::PriLow, "Read %u bytes succesffully\n", tmp);
        }

        fclose(pFile);
    }

    if (XHCI_DEBUG_MODE_LOAD_FW_ONLY & GetDebugMode())
    {
        Printf(Tee::PriNormal, "Skipping falcon launch... \n");
        PHYSADDR pPa = Platform::VirtualToPhysical(m_pDfiMem);
        Printf(Tee::PriNormal, "  Firmware binary is at 0x%llx (virtual%p) \n", pPa & 0xffffffff, m_pDfiMem);
    }
    else
    {
        CHECK_RC(LaunchFalcon(m_pDfiMem, m_DfiLength));
    }
    return OK;
}

RC XhciController::ResetFalcon()
{
    RC rc;
    if (m_pBar2)
    {
        Printf(Tee::PriLow, "ResetFalcon() not implemented yet\n");
        return OK;
    }

    UINT32 val32 = RF_NUM(LW_PROJ__XUSB_CFG_ARU_RST_CFGRST, 1);
    CHECK_RC(CfgWr32(LW_PROJ__XUSB_CFG_ARU_RST, val32));
    Printf(Tee::PriLow, "ResetFalcon() - write 0x%x to PCI CFG offset 0x%x \n", val32, LW_PROJ__XUSB_CFG_ARU_RST);
    CHECK_RC(CfgRd32(LW_PROJ__XUSB_CFG_ARU_RST, &val32));
    // check for pending bit get cleared.
    UINT32 msElapse = 0;
    while( FLD_TEST_RF_NUM(LW_PROJ__XUSB_CFG_ARU_RST_CFGRST, 1, val32) )
    {
        Tasker::Sleep(1);
        msElapse++;
        if ( msElapse > 1000 )
        {
            Printf(Tee::PriError, "ResetFalcon() - timeout with value 0x%x \n", val32);
            return RC::HW_STATUS_ERROR;
        }
        CHECK_RC(CfgRd32(LW_PROJ__XUSB_CFG_ARU_RST, &val32));
        msElapse++;
    }
    return OK;
}

RC XhciController::SaveFwLog(string FileName)
{
    RC rc = OK;
    //Avoid segmentation fault if the function is called after Controller Uninit()
    if (m_pLogBuffer == nullptr)
    {
        Printf(Tee::PriWarn, "Firmware log is not enabled\n");
        return OK;
    }

    Printf(Tee::PriNormal, "[XhciController] Writing fw logs into %s\n", FileName.c_str());
    FILE* fwLogFile;
    CHECK_RC(Utility::OpenFile(FileName, &fwLogFile, "w"));

    UINT32 countEntry = 0;
    UINT32* pDequeuePtr = static_cast<UINT32*> (m_pFwLogDeqPtr);

    for(UINT32 i = 0; i < NUM_OF_LOG_ENTRIES; i++)
    {
        if(SW_OWNED_LOG_ENTRY == (MEM_RD32(pDequeuePtr+7) >> 24))
        {
            //Read the contents into the file in a specific format

            fprintf(fwLogFile, "%08x %08x %08x %08x \n",    MEM_RD32(pDequeuePtr+0),
                                                            MEM_RD32(pDequeuePtr+1),
                                                            MEM_RD32(pDequeuePtr+2),
                                                            MEM_RD32(pDequeuePtr+3));

            fprintf(fwLogFile, "%08x %08x %08x %08x \n\n",  MEM_RD32(pDequeuePtr+4),
                                                            MEM_RD32(pDequeuePtr+5),
                                                            MEM_RD32(pDequeuePtr+6),
                                                            MEM_RD32(pDequeuePtr+7));
            //Set owner byte to 0 after reading
            UINT32 tmp = MEM_RD32(pDequeuePtr+7);
            tmp &= 0x00ffffff;
            MEM_WR32(pDequeuePtr+7, tmp);

            ++countEntry;
            pDequeuePtr += FW_LOG_ENTRY_SIZE/4;
            if ( pDequeuePtr >=
                 reinterpret_cast<UINT32*>(static_cast<UINT08*>(m_pLogBuffer) + FW_LOG_ENTRY_SIZE * NUM_OF_LOG_ENTRIES) )
            {   // ring buffer
                pDequeuePtr = static_cast<UINT32 *>(m_pLogBuffer);
            }
        }
        else
        {   // no entry left un-read
            break;
        }
    }
    // update dequeue pointer and pass it to FW
    m_pFwLogDeqPtr = static_cast<void *> (pDequeuePtr);
    PHYSADDR pPhysAddr = Platform::VirtualToPhysical(m_pFwLogDeqPtr);
    if (m_pBar2)
    {
        Bar2Wr32(XUSB_BAR2_ARU_FW_SCRATCH_0, (0x4 << 24) | (pPhysAddr & 0xffff));
        Bar2Wr32(XUSB_BAR2_ARU_FW_SCRATCH_0, (0x5 << 24) | ((pPhysAddr >> 16) & 0xffff));
    }
    else
    {
        CfgWr32(CFG_ARU_FW_SCRATCH, (0x4 << 24) | (pPhysAddr & 0xffff));
        CfgWr32(CFG_ARU_FW_SCRATCH, (0x5 << 24) | ((pPhysAddr >> 16) & 0xffff));
    }

    Printf(Tee::PriHigh, "Total FW Log Entries = %d\n", countEntry);
    fclose(fwLogFile);
    return rc;
}

RC XhciController::LaunchFalcon(void *pData, UINT32 Size)
{
    LOG_EXT();
    RC rc;

    /* Firmware logging setup */
    m_pLogBuffer = nullptr;
    CHECK_RC(MemoryTracker::AllocBufferAligned(FW_LOG_ENTRY_SIZE * NUM_OF_LOG_ENTRIES,
                                              (void**)&m_pLogBuffer,
                                              32,
                                              32,
                                              Memory::UC));
    PHYSADDR pPhysLogBuffer = Platform::VirtualToPhysical((void*)m_pLogBuffer);
    m_pFwLogDeqPtr = m_pLogBuffer;      // init the deqptr used by SaveFwLog
    Memory::Fill32(m_pLogBuffer, 0, (FW_LOG_ENTRY_SIZE * NUM_OF_LOG_ENTRIES)/4);

    UINT32 chipId = 0;
    CHECK_RC(CheetAh::GetChipId(&chipId));
    if (chipId == 0x234)
    {
        PHYSADDR pPa = Platform::VirtualToPhysical(pData);

        Printf(Tee::PriHigh, "LaunchFalcon()@0x%llx \n", pPa);

        CHECK_RC(CheetAh::PhysWr(XHCI_AO_PHYS_BASE+0x1bc, pPa & 0xFFFFFFFF));
        pPa >>= 32;
        CHECK_RC(CheetAh::PhysWr(XHCI_AO_PHYS_BASE+0x1c0, pPa & 0xFFFFFFFF));

        UINT32 data = 0;
        CHECK_RC(CheetAh::PhysRd(XHCI_AO_PHYS_BASE+0x1c4, data));
        data &= 0xFFFFFF00;
        data |= 0x7F;
        CHECK_RC(CheetAh::PhysWr(XHCI_AO_PHYS_BASE+0x1c4, data));
        CHECK_RC(CheetAh::PhysRd(XHCI_AO_PHYS_BASE+0x1c4, data));
        Printf(Tee::PriNormal, "Xhci SteamId=0x%x\n", data);

        Tasker::Sleep(500);
        return OK;
    }

    if (m_pBar2)
    {
        // write the address of the FW buffer, Falcon will start to run
        PHYSADDR pPa = Platform::VirtualToPhysical(pData);
        Printf(Tee::PriNormal, "LaunchFalcon()@0x%llx \n", pPa);

        Bar2Wr32(XUSB_BAR2_ARU_IFRDMA_CFG0, pPa);
        pPa >>= 32;
        Bar2Wr32(XUSB_BAR2_ARU_IFRDMA_CFG1, pPa);

        // program the logging buffer base address
        CHECK_RC(CsbWr(XUSB_CSB_ARU_SCRATCH0, pPhysLogBuffer));

        // program dequeue pointer to firmware
        CHECK_RC(Bar2Wr32(XUSB_BAR2_ARU_FW_SCRATCH_0, (FW_IOCTL_LOG_DEQUEUE_LOW << FW_IOCTL_TYPE_SHIFT) | (pPhysLogBuffer & 0xffff)));
        CHECK_RC(Bar2Wr32(XUSB_BAR2_ARU_FW_SCRATCH_0, (FW_IOCTL_LOG_DEQUEUE_HIGH << FW_IOCTL_TYPE_SHIFT) | ((pPhysLogBuffer >> 16) & 0xffff)));
        // program the entry number
        CHECK_RC(Bar2Wr32(XUSB_BAR2_ARU_FW_SCRATCH_0, (FW_IOCTL_LOG_BUFFER_LEN << FW_IOCTL_TYPE_SHIFT) | NUM_OF_LOG_ENTRIES));

        return OK;
    }

    UINT32 *pData32 = static_cast<UINT32 *>(pData);
    UINT32 imemOffsetDWord = 0;

    //see also dyi_layout.doc for CfgTbl definition
    UINT32 fwImemOffset = MEM_RD32(&pData32[imemOffsetDWord+0]);
    UINT32 fwCodeOffset = MEM_RD32(&pData32[imemOffsetDWord+1]);
    UINT32 fwNumTag = MEM_RD32(&pData32[imemOffsetDWord+2]);
    UINT32 fwCodeSize = MEM_RD32(&pData32[imemOffsetDWord+3]);


    // Write phys addr of logBuffer to the Fw config table.
    //MEM_WR32(&pData32[imemOffsetDWord+19], pPhysLogBuffer<<8);
    //MEM_WR32(&pData32[imemOffsetDWord+20], (pPhysLogBuffer>>24)||(NUM_OF_LOG_ENTRIES<<8));
    MEM_WR32(&pData32[imemOffsetDWord+20], pPhysLogBuffer);
    MEM_WR32(&pData32[imemOffsetDWord+21], NUM_OF_LOG_ENTRIES);

    if ( (fwImemOffset + fwCodeOffset + fwCodeSize) % 4 )
    {
        Printf(Tee::PriError, "FW DW parameters not DWord aligned \n");
        return RC::BAD_PARAMETER;
    }
    Printf(Tee::PriLow, "code size %d, code offset %d, imem offset %d, tag No. %d\n", fwCodeSize, fwCodeOffset, fwImemOffset, fwNumTag);
    if ( fwCodeSize >  Size )
    {   // simple protect
        Printf(Tee::PriError, "Bad code size %d, valid [0-%d] \n", fwCodeSize, Size);
        return RC::BAD_PARAMETER;
    }

    // handle extra memory, seems FW team is not handling this for yet
    UINT32 fwReqMemSize =  MEM_RD32(&pData32[imemOffsetDWord+5]);
    if ( fwReqMemSize & 0xffff )
    {   // allocate extra memory
        void * pTemp;
        CHECK_RC( MemoryTracker::AllocBufferAligned((fwReqMemSize&0xffff)*1024, &pTemp, 256, 32, Memory::UC) );
        m_pDfiScratchMem = pTemp;   // for later release
        MEM_WR32(&pData32[imemOffsetDWord+4], Platform::VirtualToPhysical(pTemp));
        fwReqMemSize |= (fwReqMemSize&0xffff) << 16;
        MEM_WR32(&pData32[imemOffsetDWord+5], fwReqMemSize);
    }
    else
    {
        MEM_WR32(&pData32[imemOffsetDWord+4], 0);
        MEM_WR32(&pData32[imemOffsetDWord+5], 0);
    }
    // write the HSIC present flag to notify FW about HSIC present
    UINT32 tmp =  MEM_RD32(&pData32[imemOffsetDWord+29]);
    tmp &= ~(0xff);
    tmp |= 0x1;            // set NumHSICDevicesPresent
    MEM_WR32(&pData32[imemOffsetDWord+29], tmp);

    PHYSADDR pPa = Platform::VirtualToPhysical((void *)(((UINT08 *)pData32) + XHCI_FW_CFG_TBL_SIZE));
    Printf(Tee::PriLow, "ILOAD = 0x%llx ( virt = %p ) \n", pPa, (UINT08 *)pData32 + XHCI_FW_CFG_TBL_SIZE);
    //writing start address
    CsbWr(LW_PROJ_USB3_CSB_MEMPOOL_ILOAD_BASE_LO, pPa);
    pPa >>= 32;
    if (pPa)
    {
        Printf(Tee::PriWarn, "High 32bits for ILOAD is 0x%llx, Not 0", pPa);
    }
#if defined(INCLUDE_PEATRANS)
    pPa = 0;
#endif
    CsbWr(LW_PROJ_USB3_CSB_MEMPOOL_ILOAD_BASE_HI, pPa);   // 0 is intentianlly for Peatrans
    UINT32 dummy;
    CsbRd(LW_PROJ_USB3_CSB_MEMPOOL_ILOAD_BASE_LO, &dummy);  // read it back to ensure the write

    // ------- New Scheme
    UINT32 reg = 0;
    // Program the size of DFI into ILOAD_ATTR
    CsbWr(LW_USB3_CSB_MEMPOOL_ILOAD_ATTR, Size);
    // Physical address has been written already

    // Ilwalidate l2imem. see also lwcsb.h for the constant definition
    reg = RF_NUM(LW_PROJ_USB3_CSB_MEMPOOL_L2IMEMOP_TRIG_ACTION, LW_PROJ_USB3_CSB_MEMPOOL_L2IMEMOP_TRIG_ACTION_L2IMEM_ILWALIDATE_ALL);
    CsbWr(LW_PROJ_USB3_CSB_MEMPOOL_L2IMEMOP_TRIG, reg);

    // Initiate Fetch of Bootcode from system memory into l2imem.
    // a. First Program location of BootCode and Size of Bootcode in system memory.
    reg = RF_NUM(LW_PROJ_USB3_CSB_MEMPOOL_L2IMEMOP_SIZE_SRC_OFFSET,  fwNumTag/XHCI_FW_IMEM_BLOCK_SIZE);
    reg |= RF_NUM(LW_PROJ_USB3_CSB_MEMPOOL_L2IMEMOP_SIZE_SRC_COUNT,  fwCodeSize/XHCI_FW_IMEM_BLOCK_SIZE);
    CsbWr(LW_PROJ_USB3_CSB_MEMPOOL_L2IMEMOP_SIZE, reg);

    // b. Trigger Load operation.
    reg = RF_NUM(LW_PROJ_USB3_CSB_MEMPOOL_L2IMEMOP_TRIG_ACTION, LW_PROJ_USB3_CSB_MEMPOOL_L2IMEMOP_TRIG_ACTION_L2IMEM_LOAD_LOCKED_RESULT);
    CsbWr(LW_PROJ_USB3_CSB_MEMPOOL_L2IMEMOP_TRIG, reg);

    // Program the size of autofill section
    reg = RF_NUM(LW_PFALCON_FALCON_IMFILLCTL_NBLOCKS, fwCodeSize/XHCI_FW_IMEM_BLOCK_SIZE);
    CsbWr(LW_PFALCON_FALCON_IMFILLCTL, reg);

    // Program TagLo/TagHi of autofill section.
    reg = RF_NUM(LW_PFALCON_FALCON_IMFILLRNG1_TAG_LO, fwNumTag/XHCI_FW_IMEM_BLOCK_SIZE);
    reg |= RF_NUM(LW_PFALCON_FALCON_IMFILLRNG1_TAG_HI, (fwNumTag + fwCodeSize)/XHCI_FW_IMEM_BLOCK_SIZE - 1);
    CsbWr(LW_PFALCON_FALCON_IMFILLRNG1, reg);
    // ------- Scheme end

    // Program Falcon to begin code exelwtion at PC=0.  This begins exelwtion of the boot code.
    Printf(Tee::PriLow, "Started Falcon\n");
    CsbWr(LW_USB3_FALCON_DMACTL, 0);
    CsbWr(LW_USB3_FALCON_BOOTVEC, fwNumTag);    // boot code start address
    CsbWr(LW_USB3_FALCON_CPUCTL, 0x2);

    LOG_EXT();
    return OK;
}

RC XhciController::HandleFwMsg(UINT32 TimeoutMs)
{
    RC rc;
    if (!m_pBar2 && !m_OffsetPciMailbox)
    {   // Mailbox is not enabled, do nothing
        return OK;
    }

    do
    {
        UINT32 owner = 0;
        UINT32 smi = 0;
        CHECK_RC(MailboxRegRd32(m_OffsetPciMailbox + USB3_PCI_OFFSET_OWNER, &owner));
        if ( m_pBar2 )
        {
            CHECK_RC(Bar2Rd32(XUSB_BAR2_ARU_SMI_0, &smi));
        }
        else
        {
            CHECK_RC(CfgRd32(0x428, &smi));
        }
        if ( (owner != USB3_MAILBOX_OWNER_ID_FW) || ((smi & 0x8) != 0x8) )
        {   // no Message is there, return
            Printf(Tee::PriDebug, "No Msg from firmware \n");
            break;
        }

        UINT32 message;
        CHECK_RC(MailboxRegRd32(m_OffsetPciMailbox + USB3_PCI_OFFSET_DATA_OUT, &message));
        Printf(Tee::PriLow, "Get Mailbox Msg 0x%08x \n", message);

        switch ( message >> 24 )
        {
#ifdef TEGRA_MODS
        /*
        case SW_FW_MBOX_CMD_INC_SSPI_CLOCK:
        case SW_FW_MBOX_CMD_DEC_SSPI_CLOCK:
            // see also 14.3.1  Firmware Initiating Clock Frequency Increase Increase of
            // USB3_FW_Programming_Guide_Host.doc

            CHECK_RC(ClkMgr::ClkDev()->GetRate("XUSB_SS", &reg));
            Printf(Tee::PriNormal, "SS clock %lldKHz ", reg/1000);
            CHECK_RC(ClkMgr::ClkDev()->SetDevRate("XUSB_SS", (message & 0xffffff)*1000, true));
            CHECK_RC(ClkMgr::ClkDev()->Enable("XUSB_SS", 1, 1));
            // read freq back
            CHECK_RC(ClkMgr::ClkDev()->GetRate("XUSB_SS", &reg));
            Printf(Tee::PriNormal, "-> %lldKHz ", reg/1000);

            reg = (SW_FW_MBOX_CMD_ACK << 24) | ((reg / 1000) & 0xffffff);
            CHECK_RC(MailboxTx(reg));
            break;
            */
        case SW_FW_MBOX_CMD_INC_FALC_CLOCK:
        case SW_FW_MBOX_CMD_DEC_FALC_CLOCK:
            UINT64 reg;
            CHECK_RC(ClkMgr::ClkDev()->GetRate("XUSB_FALCON", &reg));
            Printf(Tee::PriNormal, "FALCON clock %lldKHz ", reg/1000);

            CHECK_RC(ClkMgr::ClkDev()->SetDevRate("XUSB_FALCON", (message & 0xfffffff)*1000, true));
            CHECK_RC(ClkMgr::ClkDev()->Enable("XUSB_FALCON", 1, 1));
            // read freq back
            CHECK_RC(ClkMgr::ClkDev()->GetRate("XUSB_FALCON", &reg));
            Printf(Tee::PriNormal, "-> %lldKHz ", reg/1000);

            reg = (SW_FW_MBOX_CMD_ACK << 24) | ((reg / 1000) & 0xfffffff);
            CHECK_RC(MailboxTx(reg));
            break;
        case SW_FW_MBOX_CMD_ENABLE_PUPD:
            // todo: we should have a PadCtrl manager to handle it, instead of putting settings in board JS
            SysUtil::Board::ApplySettings("XUSB_HSIC_EN_PUPD");

            CHECK_RC(MailboxTx(SW_FW_MBOX_CMD_ACK<<24 | (message&0xffffff)));
            break;
        case SW_FW_MBOX_CMD_DISABLE_PUPD:
            SysUtil::Board::ApplySettings("XUSB_HSIC_DIS_PUPD");

            CHECK_RC(MailboxTx(SW_FW_MBOX_CMD_ACK<<24 | (message&0xffffff)));
            break;
        case SW_FW_MBOX_CMD_SET_BW: // just clear the Owner ID and no ACK
            CHECK_RC(MailboxRegWr32(m_OffsetPciMailbox + USB3_PCI_OFFSET_OWNER, 0));
            CHECK_RC(MailboxRegWr32(m_OffsetPciMailbox + USB3_PCI_OFFSET_MAILBOX_CMD, 1<<31));
            break;
#endif
        default:
            Printf(Tee::PriLow, "  MODS just ack it without doing anything \n");
            CHECK_RC(MailboxTx(SW_FW_MBOX_CMD_ACK<<24 | (message&0xffffff)));
            break;
        };
        // if we get a message, we wait for some time for the next one to come.
        if ( TimeoutMs )
        {
            Tasker::Sleep(TimeoutMs);
        }
    }while( true );
    return OK;
}

RC XhciController::SaveFW(UINT32 InitTag, UINT32 ByteSize)
{
    LOG_ENT();
    RC rc = OK;

    UINT32 initTag = InitTag;
    UINT32 * pData;
    CHECK_RC( MemoryTracker::AllocBuffer(ByteSize, (void**)&pData, true, 32, Memory::WB) );
    Memory::Fill08((void*)pData, 0, ByteSize);

    FILE *pFile = NULL;
    //Open a file for writing
    CHECK_RC_CLEANUP(Utility::OpenFile("fwimg.bin", &pFile, "wb"));

    for ( UINT32 i = 0; i < (ByteSize / 4); i++ )
    {
        if ( (i % 64) == 0 )
        {  // Writing imem content access tag register with block number
            CsbWr(LW_USB3_CSB_FALCON_IMEMT0, initTag);
            //Writing imem content access control reg with block number and autoincrement = 1
            CsbWr(LW_USB3_CSB_FALCON_IMEMC0, (initTag << 8) | (1 << 25));
            initTag++;
        }

        // read all the dwords within this 256 byte block
        CsbRd(LW_USB3_CSB_FALCON_IMEMD0, &(pData[i]));
    }

    CHECK_RC_CLEANUP(Utility::FWrite((void *)pData, ByteSize, 1, pFile));

Cleanup:
    if (pFile)
        fclose(pFile);
    MemoryTracker::FreeBuffer((void*)pData);

    LOG_EXT();
    return rc;
}

RC XhciController::MailboxRegRd32(const UINT32 Offset, UINT32 *pData) const
{
    if (m_pBar2)
    {
        return Bar2Rd32(Offset, pData);
    }
    return CfgRd32(Offset, pData);
}

RC XhciController::MailboxRegWr32(const UINT32 Offset, UINT32 Data) const
{
    if (m_pBar2)
    {
        return Bar2Wr32(Offset, Data);
    }
    return CfgWr32(Offset, Data);
}

RC XhciController::Bar2Rd32(const UINT32 Offset, UINT32 *pData) const
{
    if (!m_pBar2)
    {
        Printf(Tee::PriError, "BAR2 not set, call SetBar2() first\n");
        return RC::WAS_NOT_INITIALIZED;
    }

    if (Offset >= XHCI_BAR2_MMIO_ADDRESS_RANGE)
    {
        Printf(Tee::PriError, "Offset 0x%x out of range[0-0x%x] \n",
               Offset, XHCI_BAR2_MMIO_ADDRESS_RANGE);
        return RC::BAD_PARAMETER;
    }

    *pData =  MEM_RD32(static_cast<UINT08*>(m_pBar2) + Offset);
    return OK;
}

RC XhciController::Bar2Wr32(const UINT32 Offset, UINT32 Data) const
{
    if (!m_pBar2)
    {
        Printf(Tee::PriError, "BAR2 not set, call SetBar2() first\n");
        return RC::WAS_NOT_INITIALIZED;
    }

    if (Offset >= XHCI_BAR2_MMIO_ADDRESS_RANGE)
    {
        Printf(Tee::PriError, "Offset 0x%x out of range[0-0x%x] \n",
               Offset, XHCI_BAR2_MMIO_ADDRESS_RANGE);
        return RC::BAD_PARAMETER;
    }

    MEM_WR32(static_cast<UINT08*>(m_pBar2) + Offset, Data);
    return OK;
}

RC XhciController::CsbWr(UINT32 Addr, UINT32 Data32)
{
    UINT32 csbPage = (Addr & ((~0) << 9)) >> 9;

    if (m_pBar2)
    {
        Bar2Wr32(XUSB_BAR2_ARU_C11_CSBRANGE_0, csbPage);
        Bar2Wr32(XUSB_BAR2_CSB_ADDR_0 + (Addr & 0x1ff), Data32);
    }
    else
    {
        CfgWr32(MCP_USB3_CFG_ARU_C11_CSBRANGE, csbPage);
        CfgWr32(MCP_USB3_CFG_CSB_ADDR_0 + (Addr & 0x1ff), Data32);
    }
    return OK;
}

RC XhciController::CsbRd(UINT32 Addr, UINT32 *pData32)
{
    UINT32 csbPage = ( Addr & ( (~0) << 9 ) ) >> 9;

    if (m_pBar2)
    {
        Bar2Wr32(XUSB_BAR2_ARU_C11_CSBRANGE_0, csbPage);
        Bar2Rd32(XUSB_BAR2_CSB_ADDR_0 + (Addr & 0x1ff), pData32);
    }
    else
    {
        // update the Page register
        CfgWr32(MCP_USB3_CFG_ARU_C11_CSBRANGE, csbPage);
        CfgRd32(MCP_USB3_CFG_CSB_ADDR_0 +  (Addr & 0x1ff), pData32);
    }
    return OK;
}

RC XhciController::SaveXferRing(TransferRing *pXferRing)
{
    if (!pXferRing)
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    m_vpRings.push_back(pXferRing);
    return OK;
}

RC XhciController::HandleScratchBuf(bool IsAlloc)
{
    LOG_ENT();
    Printf(Tee::PriDebug,"(IsAlloc %s, Array Size %u)", IsAlloc?"T":"F", m_MaxSratchpadBufs);
    RC rc;

    if ( IsAlloc )
    {
        if ( 0 == m_MaxSratchpadBufs )
        {
            Printf(Tee::PriLow, "No Scratch Buffer needed, skip...\n");
            return OK;
        }
        void * pVa;
        // allocate Scratchpad Buffer Array
        CHECK_RC( MemoryTracker::AllocBufferAligned(8 * m_MaxSratchpadBufs, &pVa, XHCI_ALIGNMENT_SCRATCHED_BUF_ARRAY, 32, Memory::UC) );
        // fill in the PHYSADDR of Array to Device Context entry 0
        PHYSADDR pPhyAddr = Platform::VirtualToPhysical(pVa);
        if ( pPhyAddr & (XHCI_ALIGNMENT_SCRATCHED_BUF_ARRAY - 1) )
        {
            Printf(Tee::PriError, "Bad Scratchpad Buffer Array BAR 0x%llx, must be %d byte aligned \n", pPhyAddr, XHCI_ALIGNMENT_SCRATCHED_BUF_ARRAY);
            return RC::SOFTWARE_ERROR;
        }
        if ( !m_pDevContextArray )
        {
            Printf(Tee::PriError, "No Device Context Array exists \n");
            return RC::SOFTWARE_ERROR;
        }
        m_pDevContextArray->SetScratchBufArray(pPhyAddr);
        // preserve for release
        m_pScratchpadBufferArray = pVa;

        UINT32 * pData32 = (UINT32 *)m_pScratchpadBufferArray;
        // allocate Scratchpad Buffer(s) and fill its PHYSADDRs into Array
        for ( UINT32 i = 0; i < m_MaxSratchpadBufs; i++ )
        {
            CHECK_RC( MemoryTracker::AllocBufferAligned(m_PageSize, &pVa, m_PageSize, 32, Memory::UC) );
            Memory::Fill08(pVa, 0, m_PageSize);

            pPhyAddr = Platform::VirtualToPhysical(pVa);
            if ( pPhyAddr & (m_PageSize - 1) )
            {
                Printf(Tee::PriError, "Bad Scratchpad Buffer Array bar 0x%llx, must be %d byte aligned \n", pPhyAddr, m_PageSize);
                return RC::SOFTWARE_ERROR;
            }

            MEM_WR32(pData32, (UINT32)pPhyAddr);
            pPhyAddr >>= 32;
            MEM_WR32(pData32 + 1, (UINT32)pPhyAddr);
            pData32 += 2;
        }
    }
    else
    {
        if ( !m_pScratchpadBufferArray )
        {
            Printf(Tee::PriError, "No Scratchpad Buffer Array allocated \n");
            return RC::SOFTWARE_ERROR;
        }
        // clear the Device Context Entry 0
        if ( m_pDevContextArray )
            m_pDevContextArray->SetScratchBufArray(0);

        UINT32 * pData32 = (UINT32 *)m_pScratchpadBufferArray;
        for ( UINT32  i = 0; i < m_MaxSratchpadBufs; i++ )
        {
            PHYSADDR pPa = MEM_RD32(pData32 + 1);
            pPa <<= 32;
            pPa |= MEM_RD32(pData32);
            void * pVa = MemoryTracker::PhysicalToVirtual(pPa);
            MemoryTracker::FreeBuffer(pVa);
            pData32 += 2;
        }
        MemoryTracker::FreeBuffer(m_pScratchpadBufferArray);
        m_pScratchpadBufferArray = NULL;
    }

    LOG_EXT();
    return OK;
}

RC XhciController::GetEventId(UINT32 RingIndex, Tasker::EventID * pEventId)
{
    RC rc = OK;
    if ( RingIndex >= m_MaxIntrs )
    {
        Printf(Tee::PriError, "Invalid Ring Index %d, valid [0-%d] \n", RingIndex, m_MaxIntrs-1);
        return RC::BAD_PARAMETER;
    }
    if ( !pEventId )
    {
        Printf(Tee::PriError, "Null Pointer for Event ID \n");
        return RC::SOFTWARE_ERROR;
    }

    *pEventId = 0;
    if ( IRQ == m_IntrMode )
    {
        if ( RingIndex!=0 )
        {
            Printf(Tee::PriWarn, "Try to get Event %d for IRQ mode. Force Event 0", RingIndex);
        }
        *pEventId = m_Event[0];
    }
    else if ( POLL == m_IntrMode )
    {
        *pEventId = m_Event[RingIndex];
    }
    else
    {
        Printf(Tee::PriError, "BAD interrupt type %d \n", m_IntrMode);
        return RC::SOFTWARE_ERROR;
    }

    if ( (*pEventId) == NULL )
    {
        Printf(Tee::PriError, "Trying to acquire NULL event object for Evenet Ring %d \n", RingIndex);
        return RC::SOFTWARE_ERROR;
    }
    return rc;
}

RC XhciController::InitCmdRing()
{
    RC rc;

    m_pCmdRing = new CmdRing(this);
    if ( !m_pCmdRing )
    {
        Printf(Tee::PriError, "Fail to allocate memory \n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }
    UINT32 addrBit;
    CHECK_RC(GetRndAddrBits(&addrBit));
    CHECK_RC(m_pCmdRing->Init(m_CmdRingSize, addrBit));

    PHYSADDR pPhyBase;
    CHECK_RC(m_pCmdRing->GetBaseAddr(&pPhyBase));

    // write to CRCR register
    bool isCBit;
    CHECK_RC(m_pCmdRing->GetPcs(& isCBit));

    UINT32 addrLo = pPhyBase;
    pPhyBase >>= 32;
    UINT32 addrHi = pPhyBase;

    UINT32 val = 0;
    val = FLD_SET_RF_NUM(XHCI_REG_OPR_CRCR1_CRP_HI, addrHi>>(0?XHCI_REG_OPR_CRCR1_CRP_HI), val);
    CHECK_RC( RegWr(XHCI_REG_OPR_CRCR1, val) );
    val = 0;
    val = FLD_SET_RF_NUM(XHCI_REG_OPR_CRCR_CRP_LO, addrLo>>(0?XHCI_REG_OPR_CRCR_CRP_LO), val);
    val = FLD_SET_RF_NUM(XHCI_REG_OPR_CRCR_RCS, isCBit?1:0, val);
    CHECK_RC( RegWr(XHCI_REG_OPR_CRCR, val) );
    return OK;
}

RC XhciController::InitXferRing(UINT08 SlotId, UINT08 EpNum, bool IsDataOut, UINT16 StreamId)
{
    RC rc;

    Printf(Tee::PriLow, "Creating Xfer Ring for Slot %d, Ep %u, Out %s, StreamId %x\n", SlotId, EpNum, IsDataOut?"Out":"In", StreamId);

    // create new Xfer Ring
    TransferRing * pXferRing = new TransferRing(SlotId, EpNum, IsDataOut, StreamId);
    if ( !pXferRing )
    {
        Printf(Tee::PriError, "Fail to allocate memory \n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }
    UINT32 addrBit;
    CHECK_RC(GetRndAddrBits(&addrBit));
    CHECK_RC(pXferRing->Init(m_XferRingSize, addrBit));

    PHYSADDR pPhyBase;
    CHECK_RC(pXferRing->GetBaseAddr(&pPhyBase));

    UINT08 dci;
    CHECK_RC_CLEANUP(DeviceContext::EpToDci(EpNum, IsDataOut, &dci));
    if ( 0 == StreamId )
    {
        UINT08 epState;
        CHECK_RC_CLEANUP(GetEpState(SlotId, EpNum, IsDataOut, &epState));
        //if (epState != 3 && epState != 4) // if not in the Stop or Error state, send the stop endpoint command
        if (1 == epState)   // if EP is in running state
        {
            CHECK_RC_CLEANUP(CmdStopEndpoint(SlotId, dci, false));
        }
    }

    // need to attach the newly created Ring to Endpoint by calling SetDeqPtr
    // first remove the obselete one if there's any
    CHECK_RC_CLEANUP(RemoveRingObj(SlotId, XHCI_DEL_MODE_PRECISE, EpNum, IsDataOut, StreamId));

    if ( 0 == StreamId )
    {
        // then insert the new one
        CHECK_RC_CLEANUP(CmdSetTRDeqPtr(SlotId, dci, pPhyBase, true));
    }
    else
    {
        StreamContextArrayWrapper * pStreamCntxt;
        CHECK_RC_CLEANUP(FindStream(SlotId, EpNum, IsDataOut, &pStreamCntxt));
        CHECK_RC_CLEANUP(pStreamCntxt->SetRing(StreamId, pPhyBase, true));
    }

Cleanup:
    if ( OK != rc )
        delete pXferRing;
    else
        m_vpRings.push_back(pXferRing);

    return OK;
}

RC XhciController::InitEventRing(UINT32 Index)
{
    RC rc;
    if ( Index >= m_MaxIntrs )
    {
        Printf(Tee::PriError, "Invalid Interrupt Index %d, valid[0-%d] \n", Index, m_MaxIntrs-1);
        return RC::BAD_PARAMETER;
    }
    if ( m_pEventRings[Index] )
    {
        Printf(Tee::PriError, "Event Ring %d already exists \n", Index);
        return RC::BAD_PARAMETER;
    }

    UINT32 val;
    EventRing * pEventRing = new EventRing(this, Index, 1<<m_MaxEventRingSize);
    if ( !pEventRing )
    {
        Printf(Tee::PriError, "Fail to allocate memory \n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }
    UINT32 addrBit;
    CHECK_RC(GetRndAddrBits(&addrBit));
    CHECK_RC(pEventRing->Init(m_EventRingSize, addrBit));

    UINT32 tblSize;
    PHYSADDR pPhyAddr;
    // Get the base address and size
    CHECK_RC(pEventRing->GetERSTBA(&pPhyAddr, &tblSize));
    Printf(Tee::PriLow, "  Event Ring %d BAR: 0x%llx \n", Index, pPhyAddr);

    // write the size
    CHECK_RC( RegWr(XHCI_REG_ERSTSZ(Index), tblSize) );

    // program the Deq pointer, which is the seg 0 index 0 of Event Ring just created
    PHYSADDR pPhyBase;
    CHECK_RC(pEventRing->GetBaseAddr(&pPhyBase));
    CHECK_RC( RegWr(XHCI_REG_ERDP(Index), pPhyBase) );
    pPhyBase = pPhyBase >> 32;
    CHECK_RC( RegWr(XHCI_REG_ERDP1(Index), pPhyBase) );

    // write the address of Event Ring Segment Table
    CHECK_RC( RegWr(XHCI_REG_ERSTBA(Index), pPhyAddr) );
    pPhyAddr = pPhyAddr >> 32;
    CHECK_RC( RegWr(XHCI_REG_ERSTBA1(Index), pPhyAddr) );

    // set the IMOD, leave it untouched for now
    CHECK_RC( RegRd(XHCI_REG_IMOD(Index), &val) );
#ifdef SIM_BUILD
    //default value is 4000 i.e 1000uS, on simulation we set it to 40. i.e 10uS
    // see also Karthik Mani's comment on 9/3/2011 1:54 AM in bug http://lwbugs/861858
    val = FLD_SET_RF_NUM(XHCI_REG_IMOD_IMODI, 40, val);
    CHECK_RC( RegWr(XHCI_REG_IMOD(Index), val) );
#endif

    // save object for release
    m_pEventRings[Index] = pEventRing;

    // allocate event objects for this Evnet Ring
    char eventName[255];
    sprintf(eventName, "XhciEvent_At_Ring_%d", Index);
    m_Event[Index] = Tasker::AllocEvent(eventName);
    Tasker::ResetEvent(m_Event[Index]);

    //pEventRing->PrintInfo(Tee::PriNormal);
    return OK;
}

RC XhciController::InitStream(UINT08 SlotId, UINT08 Endpoint, bool IsDataOut, PHYSADDR *pPhyBase, UINT32 MaxStream)
{
    LOG_ENT();
    RC rc;
    if ( !pPhyBase )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    // delete existing one
    CHECK_RC(RemoveStreamContextArray(SlotId, false, Endpoint, IsDataOut));

    StreamContextArrayWrapper * pStreamCntxt = new StreamContextArrayWrapper(SlotId, Endpoint, IsDataOut, m_IsNss);
    m_vpStreamArray.push_back(pStreamCntxt);

    UINT32 maxStream = m_MaxPSASize;
    if (MaxStream && ( maxStream > MaxStream ))
    {
        maxStream = MaxStream;
    }

    if ( m_vUserStreamLayout.size() == 0 )
    {
        Printf(Tee::PriLow, "Dev MaxStreams = %d, HC MaxPSASize =  %d, use %d\n", MaxStream, m_MaxPSASize, maxStream);
        CHECK_RC(pStreamCntxt->Init( 0x1 << (maxStream+1) ) );
    }
    else
    {
        CHECK_RC(pStreamCntxt->Init(&m_vUserStreamLayout));
    }
    CHECK_RC(pStreamCntxt->GetBaseAddr(pPhyBase));
    LOG_EXT();
    return OK;
}

RC XhciController::EventRingSegmengSizeHandler(UINT32 EventRingIndex, UINT32 NumSegments)
{
    RegWr(XHCI_REG_ERSTSZ(EventRingIndex), NumSegments);
    return OK;
}

RC XhciController::NewEventTrbHandler(XhciTrb* pTrb)
{
    LOG_ENT();
    RC rc;
    UINT08 type;
    UINT32 val;
    CHECK_RC(TrbHelper::GetTrbType(pTrb, &type, Tee::PriDebug, true));
    if ( type == XhciTrbTypeEvCmdComplete )
    {   // save it
        PHYSADDR pCmdTrb;
        UINT08 completionCode;
        CHECK_RC(TrbHelper::ParseCmdCompleteEvent(pTrb, &completionCode, &pCmdTrb));
        XhciTrb * pVa = (XhciTrb *)MemoryTracker::PhysicalToVirtual(pCmdTrb);
        // Update Command Ring's Deq Pointer
        CHECK_RC(UpdateCmdRingDeq(pVa, XhciTrbCmpCodeCmdRingStopped != completionCode));
    }
    else if ( type == XhciTrbTypeEvTransfer )
    {
        PHYSADDR pXferTrb;
        UINT08 completionCode, slotId, dci, endpoint;
        bool isDataOut, isEventData;
        CHECK_RC(TrbHelper::ParseXferEvent(pTrb, &completionCode, &pXferTrb, NULL, &isEventData, &slotId, &dci));
        CHECK_RC(DeviceContext::DciToEp(dci, &endpoint, &isDataOut));
        // Update Xfer Ring's Deq Pointer
        // if (!isEventData)
        if ( (XhciTrbCmpCodeRingUnderrun != completionCode)
             && (XhciTrbCmpCodeRingOverrun != completionCode)
             && (XhciTrbCmpCodeIlwalidLength != completionCode) /* comes from StopEndpoint Command*/)
        {
            if ( (XhciTrbCmpCodeMissedServiceErr == completionCode)
                 && (pXferTrb == 0) )
            {
                // dummy, skip DeqPtr updating
            }
            else
            {
                CHECK_RC(UpdateXferRingDeq(&m_vpRings, slotId, endpoint, isDataOut, pXferTrb));
            }
        }
    }
    else if ( type == XhciTrbTypeEvPortStatusChg )
    {   // if the port is in U3 states, write U0 to it. (from gagarwal@lwpu.com)
        // condition: the PLC bit in PORTSC register will be set and PLS value will be F.
        // get the port ID
        CHECK_RC(TrbHelper::ParsePortStatusChgEvent(pTrb, NULL, &type));
        CHECK_RC(RegRd(XHCI_REG_PORTSC(type), &val));
        Printf(Tee::PriLow, "EventStatusChange Event for Port %d recieved, PortSc = 0x%x \n", type, val);
        if ( FLD_TEST_RF_NUM(XHCI_REG_PORTSC_PLC, 1, val) )
        {
            if ( FLD_TEST_RF_NUM(XHCI_REG_PORTSC_PLS, 0xf, val) )
            {   // write to U0
                val = FLD_SET_RF_NUM(XHCI_REG_PORTSC_PORT_LWS, 1, val);
                val = FLD_SET_RF_NUM(XHCI_REG_PORTSC_PLS, 0, val);
            }
            val = FLD_SET_RF_NUM(XHCI_REG_PORTSC_PLC, 1, val);
            val = FLD_SET_RF_NUM(XHCI_REG_PORTSC_PED, 0, val);
            Printf(Tee::PriLow, "0x%x write to RortSC for Port %d \n", val, type);
            CHECK_RC(RegWr(XHCI_REG_PORTSC(type), val));
        }
    }
    LOG_EXT();
    return OK;
}

RC XhciController::UninitRegisters()
{
    if ( m_pDummyBuffer )
    {
        MemoryTracker::FreeBuffer(m_pDummyBuffer);
        m_pDummyBuffer = nullptr;
        m_DummyBufferSize = 0;
    }

    if ( m_pDfiMem && MemoryTracker::IsValidVirtualAddress(m_pDfiMem) )
    {
        // reset Falcon to avoid it goes to indertermined state
        ResetFalcon();
    }
    return OK;
}

RC XhciController::UninitDataStructures()
{
    LOG_ENT();

    // clear the Run bit
    StartStop(false);

    if ( m_MaxSratchpadBufs )
    {   // release scratchpad buffers then scratchpad buffer array
        HandleScratchBuf(false);
    }
    // release data structures defined in InitDataStructures()
    if ( m_pCmdRing )
    {
        delete m_pCmdRing;
        m_pCmdRing = NULL;
    }

    if ( m_pEventRings[0] )
    {
        delete m_pEventRings[0];
        m_pEventRings[0] = NULL;
    }

    if ( m_pDevContextArray )
    {
        delete m_pDevContextArray;
        m_pDevContextArray = NULL;
    }

    // release data structures that will be stale when user call Xhci.Uninit()
    ReleaseDataStructures();

    LOG_EXT();
    return OK;
}

RC XhciController::IsSupport64bit(bool *pIsSupport64)
{
    LOG_ENT();
    RC rc;
    UINT32 val;

    if ( !pIsSupport64 )
    {
        Printf(Tee::PriError, "Invalid input pointer pIsSupport64 \n");
        return RC::BAD_PARAMETER;
    }

    CHECK_RC(RegRd(XHCI_REG_HCCPARAMS, &val));
    *pIsSupport64 = FLD_TEST_RF_NUM(XHCI_REG_HCCPARAMS_AC64, 1, val);

    LOG_EXT();
    return OK;
}

RC XhciController::Search(UINT32 PortNo /* =0 */, vector<UINT32> * pvPorts /*=NULL*/)
{
    RC rc = OK;
    if ( !m_MaxPorts )
    {
        Printf(Tee::PriError, "MaxPorts not set, call Init() first \n");
        return RC::WAS_NOT_INITIALIZED;
    }

    HandleFwMsg();
    bool isFind = false;
    bool isDevConn = false;

    if ( !PortNo )
    {   // a generic search
        if ( pvPorts )
        {
            pvPorts->clear();
        }
        Printf(Tee::PriNormal, "Total port %d[1-%d], Device connected on Port[", m_MaxPorts, m_MaxPorts);
        for (UINT32 i = 1; i <= m_MaxPorts; i++)
        {
            CHECK_RC(IsDevConn(i, &isDevConn));

            if (isDevConn)
            {
                Printf(Tee::PriNormal, " %d", i);
                isFind = true;
                if ( pvPorts )
                {
                    pvPorts->push_back(i);
                }
            }
        }
        if ( isFind )
        {
            Printf(Tee::PriNormal, "] \n");
        }
        else
        {
            Printf(Tee::PriNormal, "] No Device Found! \n");
        }
    }
    else
    {   // search for specified port
        if ( PortNo > m_MaxPorts )
        {
            Printf(Tee::PriError, "Invalid port number %d, valid [1,%d] \n", PortNo, m_MaxPorts);
            return RC::BAD_PARAMETER;
        }
        CHECK_RC(IsDevConn(PortNo, &isDevConn));
        if (isDevConn)
        {
            Printf(Tee::PriNormal, "Device connected on Port %d \n", PortNo);
        }
#if 0
        else
        {
            Printf(Tee::PriError, "No Device connected on Port %d \n", PortNo);
            rc = RC::HW_STATUS_ERROR;
        }
#endif
        else
        {   // not found any device
//0.if it's SS port
//1.reset the HS port.
//2.check the port connection state of SS port again, if not, report error
            // step 0
            for ( UINT32 i = 0; i < m_vPortProtocol.size(); i++ )
            {
                UINT32 tmp = m_vPortProtocol[i].CompPortOffset + m_vPortProtocol[i].CompPortCount - 1;
                if ( ( m_vPortProtocol[i].UsbVer >= 0x300 )
                     && ( PortNo >= m_vPortProtocol[i].CompPortOffset )
                     && ( PortNo <= tmp ) )
                {
                    isFind = true;  // it's on SS port
                }
            }

            // setp 1
            if ( isFind )
            {   // user is searching a SS port and not found, if its relevant HS port is connected, reset it
                UINT32 hsPortNo = (m_SSPortMap >> ((PortNo-1) * 4)) & 0xF;
                if (hsPortNo != 0x0)
                {
                    Printf(Tee::PriLow, "SSPort %d maps to HSPort %d\n", PortNo, hsPortNo);    //Print HSPort# in context of mods xhci driver
                    CHECK_RC(IsDevConn(hsPortNo, &isDevConn));
                    if(isDevConn)
                    {
                        Printf(Tee::PriNormal,
                               "  SS device not found on port %d, resetting corresponding HS port %d \n",
                               PortNo, hsPortNo);
                        ResetPort(hsPortNo);
                        Tasker::Sleep(800);
                    }
                }
            }

            // Step 2 re-check and report the status
            CHECK_RC(IsDevConn(PortNo, &isDevConn));
            if (isDevConn)
            {
                Printf(Tee::PriNormal, "Device connected on Port %d \n", PortNo);
            }
            else
            {
                Printf(Tee::PriError, "No Device connected on Port %d \n", PortNo);
                rc = RC::HW_STATUS_ERROR;
            }
        }
    }

    return rc;
}

RC XhciController::PrintState(Tee::Priority Pri) const
{
    Printf(Pri, "XHCI Controller Info :\n");
    Printf(Pri, "\tISR Counter: Triggers(%d), Serviced(%d)\n",
           m_IsrCount[ISR_EVENT_NOT_SET], m_IsrCount[ISR_EVENT_SET]);

    Printf(Pri, "\tMax Device Slots %d, Max Interrupts %d, Number of Ports %d\n",
           m_MaxSlots, m_MaxIntrs, m_MaxPorts);
    Printf(Pri, "\tMax Event Segment Table Size %d (%d items), Max Scratchpad Bufs %d\n",
           m_MaxEventRingSize, 1<<m_MaxEventRingSize, m_MaxSratchpadBufs);
    Printf(Pri, "\txHCI Extended Capabilities Pointer %dDWORD \n", m_ExtCapPtr);

    Printf(Pri, "\tCap Length: %d (0x%x)\n", m_CapLength, m_CapLength);
    Printf(Pri, "\tDoorbell Offset: 0x%x\n", m_DoorBellOffset);
    Printf(Pri, "\tRuntime Register Space: 0x%x\n", m_RuntimeOffset);

    if (m_Eci != EXTND_CAP_ID_ILWALID)
    {
        Printf(Pri, "\tEnumeration of Supported Extended Capabilities: 0x%x\n", m_Eci);
    }

    for ( UINT32 i = 0; i < m_vPortProtocol.size(); i++ )
    {
        Printf(Pri,
               "\tUSB %x, Comp Port Offset %d, Count %d",
               m_vPortProtocol[i].UsbVer,
               m_vPortProtocol[i].CompPortOffset,
               m_vPortProtocol[i].CompPortCount);
        if ( m_HcVer > 0x100 )
        {
            Printf(Pri,
                   ", Psic %d",
                   m_vPortProtocol[i].Psic);
        }
        Printf(Pri, " \n");
    }

    for ( UINT32 i = 0; i < m_MaxIntrs; i++ )
    {
        if ( m_EventCount[i] )
            Printf(Pri, "\t  from Event Ring%d: %d\n", i, m_EventCount[i]);
    }

    // print data structures
    if ( m_pDevContextArray )
    {
        Printf(Pri, "\n");
        m_pDevContextArray->PrintInfo(Pri);
    }

    for (UINT32 i = 0; i < m_MaxIntrs; i++)
    {
        if ( m_pEventRings[i] )
        {
            Printf(Pri, "\nEvent Ring Segment Table %d: \n", i);
            m_pEventRings[i]->PrintERST(Pri);
        }
    }

    PrintRingList(Pri);
    Printf(Pri, "For more details of data structures, call PrintData() \n");
    return OK;
}

RC XhciController::PrintExtDevInfo(Tee::Priority Pri) const
{
    bool isDisplayed = false;
    for ( UINT32 i = 1; i <= XHCI_MAX_SLOT_NUM; i++ )
    {
        if ( m_pUsbDevExt[i] )
        {
            isDisplayed = true;
            Printf(Pri, "Slot ID %d: \n", i);
            /*
            DeviceContext * pDevContext;
            CHECK_RC(m_pDevContextArray->GetEntry(m_SlotIdTable[i], &pDevContext));
            pDevContext->PrintInfo(Pri);
            */
            m_pUsbDevExt[i]->PrintInfo(Pri);
        }
    }
    if ( !isDisplayed )
    {
        Printf(Pri, "No USB device enumerated yet \n");
    }
    return OK;
}

RC XhciController::ToggleControllerInterrupt(bool IsEn)
{
    if (IsEn)
    {
        return DeviceEnableInterrupts();
    }
    return DeviceDisableInterrupts();
}

RC XhciController::DeviceEnableInterrupts()
{
    LOG_ENT();
    RC rc;
    UINT32 val32;

    // clear interrupt bit set before this funtion while turn on per interrupt IE
    for ( UINT32 i = 0; i < m_MaxIntrs; i++ )
    {
        CHECK_RC(RegRd(XHCI_REG_IMAN(i), &val32));
        val32 = FLD_SET_RF_NUM(XHCI_REG_IMAN_IP, 0x1, val32);   // Write 1 to clear
        val32 = FLD_SET_RF_NUM(XHCI_REG_IMAN_IE, 0x1, val32);   // Enable the IE
        CHECK_RC(RegWr(XHCI_REG_IMAN(i), val32));

        CHECK_RC(RegRd(XHCI_REG_ERDP(i), &val32));
        // EHB might be set when IP is set. so clear EHB along with IP
        val32 = FLD_SET_RF_NUM(XHCI_REG_ERDP_EHB, 0x1, val32);
        CHECK_RC(RegWr(XHCI_REG_ERDP(i), val32));
    }

    CHECK_RC(RegRd(XHCI_REG_OPR_USBCMD, &val32));
    val32 = FLD_SET_RF_NUM(XHCI_REG_OPR_USBCMD_INTE, 0x1, val32);
    CHECK_RC(RegWr(XHCI_REG_OPR_USBCMD, val32));

    LOG_EXT();
    return OK;
}

RC XhciController::DeviceDisableInterrupts()
{
    LOG_ENT();
    RC rc;

    UINT32 val32;
    CHECK_RC(RegRd(XHCI_REG_OPR_USBCMD, &val32));
    val32 = FLD_SET_RF_NUM(XHCI_REG_OPR_USBCMD_INTE, 0x0, val32);
    CHECK_RC(RegWr(XHCI_REG_OPR_USBCMD, val32));

    // clear the EINT
    CHECK_RC(RegRd(XHCI_REG_OPR_USBSTS, &val32));
    val32 = FLD_SET_RF_NUM(XHCI_REG_OPR_USBSTS_EINT, 0x1, val32);
    CHECK_RC(RegWr(XHCI_REG_OPR_USBSTS, val32));

    for ( UINT32 i = 0; i < m_MaxIntrs; i++ )
    {
        CHECK_RC(RegRd(XHCI_REG_IMAN(i), &val32));
        val32 = FLD_SET_RF_NUM(XHCI_REG_IMAN_IP, 0x1, val32);   // Write 1 to clear
        val32 = FLD_SET_RF_NUM(XHCI_REG_IMAN_IE, 0x0, val32);   // Disable the IE
        CHECK_RC(RegWr(XHCI_REG_IMAN(i), val32));
    }

    LOG_EXT();
    return OK;
}

long XhciController::Isr()
{
    UINT32 val32;
    UINT32 iman;
    long result = Xp::ISR_RESULT_NOT_SERVICED;

    m_IsrCount[ISR_EVENT_NOT_SET]++;
    RegRd(XHCI_REG_IMAN(0), &iman);
    if ( FLD_TEST_RF_NUM(XHCI_REG_IMAN_IP, 0x1, iman) )
    {
        // turn off interrupt
        RegRd(XHCI_REG_OPR_USBCMD, &val32);
        val32 = FLD_SET_RF_NUM(XHCI_REG_OPR_USBCMD_INTE, 0x0, val32);
        RegWr(XHCI_REG_OPR_USBCMD, val32);

        Tasker::SetEvent(m_Event[0]);
        m_EventCount[0]++;      // legacy IRQ comes only from Interrupt 0
        m_IsrCount[ISR_EVENT_SET]++;
        result = Xp::ISR_RESULT_SERVICED;

        // clear the IP bit
        iman = FLD_SET_RF_NUM(XHCI_REG_IMAN_IP, 0x1, iman);
        RegWr(XHCI_REG_IMAN(0), iman);

        // turn interrupt back on
        RegRd(XHCI_REG_OPR_USBCMD, &val32);
        val32 = FLD_SET_RF_NUM(XHCI_REG_OPR_USBCMD_INTE, 0x1, val32);
        RegWr(XHCI_REG_OPR_USBCMD, val32);

        // read back to make sure write take effect
        RegRd(XHCI_REG_IMAN(0), &iman);
    }
    return result;
}

RC XhciController::PrintReg(Tee::Priority Pri) const
{
    PrintRegGlobal(Pri);
    PrintRegRuntime(Pri);
    return OK;
}

RC XhciController::PrintRegGlobal(Tee::Priority PriRaw, Tee::Priority PriInfo) const
{
    UINT32 val, val1;
    RC rc;

    Printf(PriRaw, "XHCI Controller Capability Regs:\n");
    CHECK_RC(RegRd(XHCI_REG_CAPLENGTH, &val));
    UINT32 capLength = val & 0xff;
    Printf(PriRaw, " (0x%02x) CAPLENGTH = 0x%08x \t", XHCI_REG_CAPLENGTH, val);
    Printf(PriRaw, " (0x%02x) HCIVERSION = 0x%04x \n", XHCI_REG_HCIVERSION, val>>16);

    CHECK_RC(RegRd(XHCI_REG_HCSPARAMS1, &val));
    Printf(PriRaw, " (0x%02x) HCSPARAMS 1 = 0x%08x \t", XHCI_REG_HCSPARAMS1, val);
    Printf(PriInfo, "\n  MaxSlot %d, Max Interrupters %d, Max Ports %d \n",
           RF_VAL(XHCI_REG_HCSPARAMS1_MAXSLOTS, val),
           RF_VAL(XHCI_REG_HCSPARAMS1_MAXINTRS, val),
           RF_VAL(XHCI_REG_HCSPARAMS1_MAXPORTS, val));

    CHECK_RC(RegRd(XHCI_REG_HCSPARAMS2, &val));
    Printf(PriRaw, " (0x%02x) HCSPARAMS 2 = 0x%08x \n", XHCI_REG_HCSPARAMS2, val);
    Printf(PriInfo, "  IST %d, ERST Max %d, IOC Interval %d, SPR %d, Max Scratchpad Bufs Hi 0x%x / Lo 0x%x \n",
           RF_VAL(XHCI_REG_HCSPARAMS2_IST, val),
           RF_VAL(XHCI_REG_HCSPARAMS2_ERSTMAX, val),
           RF_VAL(XHCI_REG_HCSPARAMS2_IOCINTERVAL, val),
           RF_VAL(XHCI_REG_HCSPARAMS2_SPR, val),
           RF_VAL(XHCI_REG_HCSPARAMS2_MAXSCRATCHPADBUFS_HI, val),
           RF_VAL(XHCI_REG_HCSPARAMS2_MAXSCRATCHPADBUFS_LO, val));

    CHECK_RC(RegRd(XHCI_REG_HCSPARAMS3, &val));
    Printf(PriRaw, " (0x%02x) HCSPARAMS 3 = 0x%08x \t", XHCI_REG_HCSPARAMS3, val);
    Printf(PriInfo, "\n  U1 Device Exit Latency %d, U2 Device Exit Latency %d \n",
           RF_VAL(XHCI_REG_HCSPARAMS3_U1, val),
           RF_VAL(XHCI_REG_HCSPARAMS3_U2, val));

    CHECK_RC(RegRd(XHCI_REG_HCCPARAMS, &val));
    Printf(PriRaw, " (0x%02x) HCCPARAMS = 0x%08x \n", XHCI_REG_HCCPARAMS, val);
    Printf(PriInfo, "  AC64 %d, BNC %d, CSZ %d, PPC %d, PIND %d, LHRC %d, LTC %d, NSS %d, FSE: %d, SBD %d, MaxPSASize %d, xECP %d \n",
           RF_VAL(XHCI_REG_HCCPARAMS_AC64, val),
           RF_VAL(XHCI_REG_HCCPARAMS_BNC, val),
           RF_VAL(XHCI_REG_HCCPARAMS_CSZ, val),
           RF_VAL(XHCI_REG_HCCPARAMS_PPC, val),
           RF_VAL(XHCI_REG_HCCPARAMS_PIND, val),
           RF_VAL(XHCI_REG_HCCPARAMS_LHRC, val),
           RF_VAL(XHCI_REG_HCCPARAMS_LTC, val),
           RF_VAL(XHCI_REG_HCCPARAMS_NSS, val),
           RF_VAL(XHCI_REG_HCCPARAMS_FSE, val),
           RF_VAL(XHCI_REG_HCCPARAMS_SBD, val),
           RF_VAL(XHCI_REG_HCCPARAMS_MAXPSASIZE, val),
           RF_VAL(XHCI_REG_HCCPARAMS_XEPC, val));

    CHECK_RC(RegRd(XHCI_REG_HCCPARAMS2, &val));
    Printf(PriRaw, " (0x%02x) HCCPARAMS 2= 0x%08x \n", XHCI_REG_HCCPARAMS2, val);
    Printf(PriInfo, " U3C %d, CMC %d, FSC %d, CTC %d, LEC %d, CIC %d, ETC %d, TSC %d, GSC %d, VTC %d\n",
           RF_VAL(XHCI_REG_HCCPARAMS2_U3C, val),
           RF_VAL(XHCI_REG_HCCPARAMS2_CMC, val),
           RF_VAL(XHCI_REG_HCCPARAMS2_FSC, val),
           RF_VAL(XHCI_REG_HCCPARAMS2_CTC, val),
           RF_VAL(XHCI_REG_HCCPARAMS2_LEC, val),
           RF_VAL(XHCI_REG_HCCPARAMS2_CIC, val),
           RF_VAL(XHCI_REG_HCCPARAMS2_ETC, val),
           RF_VAL(XHCI_REG_HCCPARAMS2_TSC, val),
           RF_VAL(XHCI_REG_HCCPARAMS2_GSC, val),
           RF_VAL(XHCI_REG_HCCPARAMS2_VTC, val));

    CHECK_RC(RegRd(XHCI_REG_DBOFF, &val));
    Printf(PriRaw, " (0x%02x) Doorbell Offset = 0x%08x \n", XHCI_REG_DBOFF, val);

    CHECK_RC(RegRd(XHCI_REG_RTSOFF, &val));
    Printf(PriRaw, " (0x%02x) Runtime Register Space Offset = 0x%08x \n", XHCI_REG_RTSOFF, val);

    //--------------------------------------------------------------------------
    Printf(PriRaw, "XHCI Operational Regs:\n");
    CHECK_RC(RegRd(XHCI_REG_OPR_USBCMD, &val));
    Printf(PriRaw, " (0x%02x) USBCMD = 0x%08x \t", (XHCI_REG_OPR_USBCMD & ~XHCI_REG_TYPE_OPERATIONAL) + capLength, val);
    Printf(PriInfo,
           "\n  Run/Stop = %d, Host Controller Reset: %s "
           "\n  Interrupter Enable = %d, Host System Error Enable = %d "
           "\n  Light Host Controller Reset = %d, Controller Save State = %d "
           "\n  Controller Restore State = %d, Enable Wrap Event = %d \n",
           RF_VAL(XHCI_REG_OPR_USBCMD_RS, val), FLD_TEST_RF_NUM(XHCI_REG_OPR_USBCMD_HCRST,1,val)?"Yes":"No",
           RF_VAL(XHCI_REG_OPR_USBCMD_INTE, val), RF_VAL(XHCI_REG_OPR_USBCMD_HSEE, val),
           RF_VAL(XHCI_REG_OPR_USBCMD_LHCRST, val), RF_VAL(XHCI_REG_OPR_USBCMD_CSS, val),
           RF_VAL(XHCI_REG_OPR_USBCMD_CRS, val), RF_VAL(XHCI_REG_OPR_USBCMD_EWE, val));

    CHECK_RC(RegRd(XHCI_REG_OPR_USBSTS, &val));
    Printf(PriRaw, " (0x%02x) USBSTS = 0x%08x \n", (XHCI_REG_OPR_USBSTS & ~XHCI_REG_TYPE_OPERATIONAL) + capLength, val);
    Printf(PriInfo,
           "  HC Halted = %d, Host System Error = %d, Event Interrupt %d "
           "\n  Port Change Detect = %d "
           "\n  Save State Status = %d, Restore State Status = %d, Save/Restore Error %d "
           "\n  Controller Not Ready = %d, Host Controller Error = %d \n",
           RF_VAL(XHCI_REG_OPR_USBSTS_HCH, val), RF_VAL(XHCI_REG_OPR_USBSTS_HSE, val), RF_VAL(XHCI_REG_OPR_USBSTS_EINT, val),
           RF_VAL(XHCI_REG_OPR_USBSTS_PCD,val),
           RF_VAL(XHCI_REG_OPR_USBSTS_SSS, val), RF_VAL(XHCI_REG_OPR_USBSTS_RSS, val), RF_VAL(XHCI_REG_OPR_USBSTS_SRE, val),
           RF_VAL(XHCI_REG_OPR_USBSTS_CNR, val), RF_VAL(XHCI_REG_OPR_USBSTS_HCE, val));

    CHECK_RC(RegRd(XHCI_REG_OPR_PAGESIZE, &val));
    Printf(PriRaw, " (0x%02x) PAGESIZE = 0x%08x (%d Bytes)\n",
           (XHCI_REG_OPR_PAGESIZE & ~XHCI_REG_TYPE_OPERATIONAL) + capLength, val,
           (1<<12) * RF_VAL(XHCI_REG_OPR_PAGESIZE_PS, val));

    CHECK_RC(RegRd(XHCI_REG_OPR_DNCTRL, &val));
    Printf(PriRaw, " (0x%02x) Device Notification Control = 0x%08x \n", (XHCI_REG_OPR_DNCTRL & ~XHCI_REG_TYPE_OPERATIONAL) + capLength, val);

    CHECK_RC(RegRd(XHCI_REG_OPR_CRCR, &val));
    CHECK_RC(RegRd(XHCI_REG_OPR_CRCR1, &val1));
    Printf(PriRaw, " (0x%02x) Command Ring Control = 0x%08x \n", (XHCI_REG_OPR_CRCR & ~XHCI_REG_TYPE_OPERATIONAL) + capLength, val);
    Printf(PriInfo,"  Note: CRCR is all 0 on read. \n  Ring Cycle State %d, Command Stop %d, Command Abort %d, Command Ring Running %d ",
           RF_VAL(XHCI_REG_OPR_CRCR_RCS, val),
           RF_VAL(XHCI_REG_OPR_CRCR_CS, val),
           RF_VAL(XHCI_REG_OPR_CRCR_CA, val),
           RF_VAL(XHCI_REG_OPR_CRCR_CCR, val));
    Printf(PriInfo,"\n  Command Ring Pointer = 0x%08x", RF_VAL(XHCI_REG_OPR_CRCR1_CRP_HI, val1));
    Printf(PriInfo,"%08x \n", RF_VAL(XHCI_REG_OPR_CRCR_CRP_LO, val) << (0?XHCI_REG_OPR_CRCR_CRP_LO));

    CHECK_RC(RegRd(XHCI_REG_OPR_DCBAAP_LO, &val));
    CHECK_RC(RegRd(XHCI_REG_OPR_DCBAAP_HI, &val1));
    Printf(PriRaw, " (0x%02x) Device Context Base Address Array Pointer = 0x%08x", (XHCI_REG_OPR_DCBAAP_LO & ~XHCI_REG_TYPE_OPERATIONAL) + capLength, val1);
    Printf(PriRaw, "%08x \n", val);

    CHECK_RC(RegRd(XHCI_REG_OPR_CONFIG, &val));
    Printf(PriRaw, " (0x%02x) Configure Register = 0x%08x \n", (XHCI_REG_OPR_CONFIG & ~XHCI_REG_TYPE_OPERATIONAL) + capLength, val);
    Printf(PriInfo,"  Number of Device Slots Enabled %u \n", RF_VAL(XHCI_REG_OPR_CONFIG_MAXSLOTEN, val));

    Printf(PriRaw, "\n");
    return OK;
}

RC XhciController::SetU1U2Timeout(UINT32 Port, UINT32 U1Timeout, UINT32 U2Timeout)
{
    RC rc;
    UINT32 val;

    CHECK_RC(RegRd(XHCI_REG_PORTPMSC(Port), &val));
    val = FLD_SET_RF_NUM(XHCI_REG_PORTPMSC_U1_TIMEOUT, U1Timeout, val);
    val = FLD_SET_RF_NUM(XHCI_REG_PORTPMSC_U2_TIMEOUT, U2Timeout, val);
    CHECK_RC(RegWr(XHCI_REG_PORTPMSC(Port), val));

    return OK;
}

RC XhciController::SetL1DevSlot(UINT32 Port, UINT32 SlotId /*= 0*/)
{
    RC rc;
    UINT32 val;

    if (SlotId == 0)
    {
        CHECK_RC(GetSlotIdByPort(Port, &SlotId));
    }
    CHECK_RC(RegRd(XHCI_REG_PORTPMSC(Port), &val));
    val = FLD_SET_RF_NUM(XHCI_REG_PORTPMSC_L1_DEVICE_SLOT, SlotId, val);
    CHECK_RC(RegWr(XHCI_REG_PORTPMSC(Port), val));

    CHECK_RC(RegRd(XHCI_REG_PORTPMSC(Port), &val));
    if (!FLD_TEST_RF_NUM(XHCI_REG_PORTPMSC_L1_DEVICE_SLOT, SlotId, val))
    {
        Printf(Tee::PriNormal,
               "Fail to set L1 device slot for port %d, slot-id %d\n",
               Port,
               SlotId);
        return RC::HW_STATUS_ERROR;
    }

    return OK;
}

RC XhciController::EnableL1Status(UINT32 Port,
                                  UINT32 TimeoutMs /*= 1000*/)
{
    LOG_ENT();
    RC rc = OK;
    UINT32 count = 0;
    UINT32 val = 0;
    UINT32 status = 0;

    // set the L1 device slot before entering L1 mode
    CHECK_RC(SetL1DevSlot(Port));

    do
    {
        // send L1 request
        CHECK_RC(PortPLS(Port, nullptr, XHCI_REG_PORTSC_PLS_VAL_L1));
        Tasker::Sleep(100); // incase read invalid L1S back if we read L1S too fast
        count += 100;

        // check L1S
        CHECK_RC(RegRd(XHCI_REG_PORTPMSC(Port), &val));
        status = RF_VAL(XHCI_REG_PORTPMSC_L1S, val);

        if (status == L1S_SUCCESS)
        {
            Printf(Tee::PriLow, "Device on port %d enter L1 mode\n", Port);
            break;
        }

        if (status != L1S_NOT_YET)
        {
            Printf(Tee::PriError, "Error with L1 status reported: %d\n", status);
            return RC::HW_STATUS_ERROR;
        }

        // For L1S_NOT_YET: device is not ready to enter L1 mode yet
        // host re-send L1 request again
    } while (count < TimeoutMs);

    LOG_EXT();
    return rc;
}

RC XhciController::WaitPLS(UINT32 Port, UINT32 ExpPls, UINT32 TimeoutMs)
{
    UINT32 regOff = XHCI_REG_PORTSC(Port);
    regOff &= ~XHCI_REG_TYPE_OPERATIONAL;
    regOff += m_CapLength;
    Printf(Tee::PriLow, "wait PLS for Port %d @ register 0x%x\n", Port, regOff);
    return WaitRegBits(regOff, 0?XHCI_REG_PORTSC_PLS, 1?XHCI_REG_PORTSC_PLS, ExpPls, TimeoutMs);
}

RC XhciController::PortPLS(UINT32 Port, UINT32 *pData, UINT32 Pls, bool IsClearPlc)
{
    RC rc;
    UINT32 val;
    CHECK_RC(RegRd(XHCI_REG_PORTSC(Port), &val));
    val &= ~XHCI_REG_PORTSC_WORKING_MASK;

    if ( IsClearPlc )
    {
        val = FLD_SET_RF_NUM(XHCI_REG_PORTSC_PLC, 0x1, val);
    }

    if ( pData )
    {   // caller just wants to read the PLS and return
        *pData = RF_VAL(XHCI_REG_PORTSC_PLS, val);
        return OK;
    }

    val = FLD_SET_RF_NUM(XHCI_REG_PORTSC_PLS, Pls,  val);
    val = FLD_SET_RF_NUM(XHCI_REG_PORTSC_PORT_LWS, 0x1, val);
    Printf(Tee::PriLow, "Write 0x%08x to PortSc of port %d \n", val, Port);
    CHECK_RC(RegWr(XHCI_REG_PORTSC(Port), val));

    return OK;
}

RC XhciController::PortPP(UINT32 Port, bool IsClearPP, UINT32 TimeoutMs)
{
    RC rc;
    UINT32 val;
    UINT32 regOff;
    CHECK_RC(RegRd(XHCI_REG_PORTSC(Port), &val));
    val &= ~XHCI_REG_PORTSC_WORKING_MASK;

    if ( !IsClearPP )
    {
        val = FLD_SET_RF_NUM(XHCI_REG_PORTSC_PP, 0x1, val);
    }
    else
    {
        val = FLD_SET_RF_NUM(XHCI_REG_PORTSC_PP, 0x0, val);
    }

    Printf(Tee::PriLow, "Write 0x%08x to PortSc of port %d \n", val, Port);
    CHECK_RC(RegWr(XHCI_REG_PORTSC(Port), val));

    /*
    After modifying PP, software shall read PP and confirm that it's reached its target state before
    modifying it again, undefined behaviour may occur if this procedure is not followed. - xHCI 1.2 page 409
    */
    regOff = XHCI_REG_PORTSC(Port);
    regOff &= ~XHCI_REG_TYPE_OPERATIONAL;
    regOff += m_CapLength;
    Printf(Tee::PriLow, "wait PP for Port %d @ register 0x%x\n", Port, regOff);
    CHECK_RC(WaitRegBits(regOff, 0?XHCI_REG_PORTSC_PP, 1?XHCI_REG_PORTSC_PP, val, TimeoutMs));

    return OK;
}

RC XhciController::PrintRegPort(UINT32 Port, Tee::Priority PriRaw, Tee::Priority PriInfo)
{    //Port = Port Number (Valid values are 1, 2, 3, ... MaxPorts)
    RC rc;
    const char * strPortLinkState[] = {"U0 State","U1 State","U2 State","U3 State/ Suspend","Disabled","RxDetect","Inactive","Polling",
        "Recovery","Hot Reset","Compliance Mode","Loopback State","Reserved","Reserved","Reserved","Reserved"};
    const char * strSpeed[] = {"Undefined","Full-speed","Low-speed","High-speed","SuperSpeed",
        "Reserved","Reserved","Reserved","Reserved","Reserved",
        "Reserved","Reserved","Reserved","Reserved","Reserved",
        "Unknown/Error"};

    if ( Port <1 || Port > m_MaxPorts )
    {
        Printf(Tee::PriError, "Invalid port number %d, valid [1,%d] \n", Port, m_MaxPorts);
        return RC::SOFTWARE_ERROR;
    }

    UINT32 val;
    Printf(PriRaw, "XHCI Port Status and Control Register for Port %d: \n", Port);
    CHECK_RC(RegRd(XHCI_REG_PORTSC(Port), &val));
    Printf(PriRaw, " (0x%02x) PORTSC = 0x%08x \n", (XHCI_REG_PORTSC(Port) & ~XHCI_REG_TYPE_OPERATIONAL) + m_CapLength, val);
    Printf(PriInfo,
           "  Current Connect Status = %d, Port %s, Over-current Active %d "
           "\n  Port Reset = %d, Port Link State = %d (%s) "
           "\n  Port Power = %d Port Speed = %d (%s) "
           "\n  Port Indicator Control = %d, Port Link State Write Strobe = %d "
           "\n  Connect Status Change = %d, Port Enable/Disable Change = %d "
           "\n  Warm Port Reset Change = %d "
           "\n  Over-current Change = %d, Port Reset Change = %d "
           "\n  Port Link State Change = %d, Port CFG Err Change = %d "
           "\n  Wake on Connect Enable = %d, Wake on Disconnect Enable = %d "
           "\n  Wake on Over-current Enable = %d "
           "\n  Device Removable %s, Warm Port Reset = %d \n",
           RF_VAL(XHCI_REG_PORTSC_CCS, val), FLD_TEST_RF_NUM(XHCI_REG_PORTSC_PED, 1, val)?"Enabled":"Disabled", RF_VAL(XHCI_REG_PORTSC_OCA, val),
           RF_VAL(XHCI_REG_PORTSC_PR, val), RF_VAL(XHCI_REG_PORTSC_PLS, val), strPortLinkState[RF_VAL(XHCI_REG_PORTSC_PLS, val)],
           RF_VAL(XHCI_REG_PORTSC_PP, val), RF_VAL(XHCI_REG_PORTSC_PORT_SPEED, val), strSpeed[RF_VAL(XHCI_REG_PORTSC_PORT_SPEED, val)],
           RF_VAL(XHCI_REG_PORTSC_PORT_PIC, val), RF_VAL(XHCI_REG_PORTSC_PORT_LWS, val),
           RF_VAL(XHCI_REG_PORTSC_CSC, val), RF_VAL(XHCI_REG_PORTSC_PEC, val), RF_VAL(XHCI_REG_PORTSC_WRC, val),
           RF_VAL(XHCI_REG_PORTSC_OCC, val), RF_VAL(XHCI_REG_PORTSC_PRC, val), RF_VAL(XHCI_REG_PORTSC_PLC, val), RF_VAL(XHCI_REG_PORTSC_CEC, val),
           RF_VAL(XHCI_REG_PORTSC_WCE, val), RF_VAL(XHCI_REG_PORTSC_WDE, val), RF_VAL(XHCI_REG_PORTSC_WOE, val),
           FLD_TEST_RF_NUM(XHCI_REG_PORTSC_DR, 0, val)?"True":"False", RF_VAL(XHCI_REG_PORTSC_WPR, val));

    Printf(PriRaw, "Port PM Status and Control Register: \n");
    CHECK_RC(RegRd(XHCI_REG_PORTPMSC(Port), &val));
    Printf(PriRaw, " (0x%02x) PORTPMSC = 0x%08x \n", (XHCI_REG_PORTPMSC(Port) & ~XHCI_REG_TYPE_OPERATIONAL) + m_CapLength, val);

    Printf(PriRaw, "Port Link Info Register: \n");
    CHECK_RC(RegRd(XHCI_REG_PORTLI(Port), &val));
    Printf(PriRaw, " (0x%02x) PORTLI = 0x%08x \n", (XHCI_REG_PORTLI(Port) & ~XHCI_REG_TYPE_OPERATIONAL) + m_CapLength, val);

    return OK;
}

RC XhciController::PrintRegRuntime(Tee::Priority PriRaw, Tee::Priority PriInfo) const
{
    RC rc;

    UINT32 val, val1;
    Printf(PriRaw, "XHCI Runtime Registers: \n");
    CHECK_RC(RegRd(XHCI_REG_MFINDEX, &val));
    Printf(PriRaw, " (0x%02x) Microframe Index = 0x%08x \n", (XHCI_REG_MFINDEX & ~XHCI_REG_TYPE_RUNTIME) + m_RuntimeOffset, val);

    for ( UINT32 i = 0; i <m_MaxIntrs; i++ )
    {
        CHECK_RC(RegRd(XHCI_REG_ERSTSZ(i), &val));
        if ( FLD_TEST_RF_NUM(XHCI_REG_ERSTSZ_BITS, 0, val) )
            continue;       // skip disabled entries

        Printf(PriRaw, "Interrupter Register Set %d (out of %d) \n", i, m_MaxIntrs);
        CHECK_RC(RegRd(XHCI_REG_IMAN(i), &val));
        Printf(PriRaw, " (0x%02x) Interrupter Management = 0x%08x \n", (XHCI_REG_IMAN(i) & ~XHCI_REG_TYPE_RUNTIME) + m_RuntimeOffset, val);
        CHECK_RC(RegRd(XHCI_REG_IMOD(i), &val));
        Printf(PriRaw, " (0x%02x) Interrupter Moderation = 0x%08x \n", (XHCI_REG_IMOD(i) & ~XHCI_REG_TYPE_RUNTIME) + m_RuntimeOffset, val);
        CHECK_RC(RegRd(XHCI_REG_ERSTSZ(i), &val));
        Printf(PriRaw, " (0x%02x) Event Ring Segment Table Size = 0x%08x \n", (XHCI_REG_ERSTSZ(i) & ~XHCI_REG_TYPE_RUNTIME) + m_RuntimeOffset, val);
        CHECK_RC(RegRd(XHCI_REG_ERSTBA(i), &val));
        CHECK_RC(RegRd(XHCI_REG_ERSTBA1(i), &val1));
        Printf(PriRaw, " (0x%02x) Event Ring Segment Table Base Address = 0x%08x%08x \n", (XHCI_REG_ERSTBA(i) & ~XHCI_REG_TYPE_RUNTIME) + m_RuntimeOffset, val1, val);
        CHECK_RC(RegRd(XHCI_REG_ERDP(i), &val));
        CHECK_RC(RegRd(XHCI_REG_ERDP1(i), &val1));
        Printf(PriRaw, " (0x%02x) Event Ring Dequeue Pointer = 0x%08x%08x \n", (XHCI_REG_ERDP(i) & ~XHCI_REG_TYPE_RUNTIME) + m_RuntimeOffset, val1, val);
    }
    Printf(PriRaw, "\n");
    return OK;
}

RC XhciController::PrintData(UINT08 Type, UINT08 Index /* = 0*/)
{
    RC rc;
    if ( Type == 0 )
    {
        if ( !m_pDevContextArray )
        {
            Printf(Tee::PriError, "Device Context Base Address Array not initialized yet \n");
            return RC::WAS_NOT_INITIALIZED;
        }
        m_pDevContextArray->PrintInfo(Tee::PriNormal);
    }
    else if ( Type == 1 )
    {
        if ( Index == 0 )
        {
            Printf(Tee::PriError, "Invalid index 0, valid [1-255] \n");
            return RC::BAD_PARAMETER;
        }
        if ( !m_pDevContextArray )
        {
            Printf(Tee::PriError, "Device Context Base Address Array not initialized yet \n");
            return RC::WAS_NOT_INITIALIZED;
        }
        m_pDevContextArray->PrintInfo(Tee::PriNormal, Index);
    }
    else if ( Type == 2 )
    {
        if ( Index == 0 )
        {
            Printf(Tee::PriError, "Invalid address 0, valid [1-255] \n");
            return RC::BAD_PARAMETER;
        }
        CHECK_RC(CheckDevValid(Index));
        m_pUsbDevExt[Index]->PrintInfo(Tee::PriNormal);
    }
    else if ( Type == 3 )
    {
        if ( Index >= m_MaxIntrs )
        {
            Printf(Tee::PriError, "Invalid index %u, valid [0-%u] \n", Index, m_MaxIntrs - 1);
            return RC::BAD_PARAMETER;
        }
        if ( !m_pEventRings[Index] )
        {
            Printf(Tee::PriError, "Event Ring %d does not exist\n", Index);
            return RC::BAD_PARAMETER;
        }
        m_pEventRings[Index]->PrintERST(Tee::PriNormal);
    }
    else if ( Type == 4 )
    {
        if ( Index >= m_vpStreamArray.size() )
        {
            Printf(Tee::PriError, "Invalid index %u, num of StreamContextArray exits %u \n", Index, (UINT32)m_vpStreamArray.size());
            return RC::BAD_PARAMETER;
        }
        m_vpStreamArray[Index]->PrintInfo(Tee::PriNormal);
    }
    else if ( Type == 5 )
    {
        if ( 0 == m_vpRings.size() )
        {
            Printf(Tee::PriNormal, "No Xfer Ring found \n");
        }
        else
        {
            for ( UINT32 i = 0; i<m_vpRings.size(); i++ )
            {
                m_vpRings[i]->PrintInfo(Tee::PriNormal);
            }
        }
    }
    else if ( Type == 6 )
    {   // print Scracthpad buffer
        if ( NULL == m_pScratchpadBufferArray )
        {
            Printf(Tee::PriNormal, "No Scratchpad Buffer exists \n");
            return OK;

        }
        UINT32 * pData32 = (UINT32 *)m_pScratchpadBufferArray;
        PHYSADDR pPa = 0;
        void * pVa = NULL;
        for ( UINT32  i = 0; i < m_MaxSratchpadBufs; i++ )
        {
            Printf(Tee::PriNormal, "Scratchpad Buffer Page %d of %d \n", i, m_MaxSratchpadBufs);
            pPa = MEM_RD32(pData32 + 1);
            pPa <<= 32;
            pPa |= MEM_RD32(pData32);

            pVa = MemoryTracker::PhysicalToVirtual(pPa);
            Memory::Print32(pVa, m_PageSize);

            pData32 += 2;
        }
        Printf(Tee::PriNormal, "End of Scratchpad Buffer dump \n");
    }
    else
    {
        Printf(Tee::PriError, "Bad type %d \n", Type);
        return RC::BAD_PARAMETER;
    }
    return OK;
}

RC XhciController::PrintRingList(Tee::Priority Pri) const
{
    RC rc;
    bool isRingExist = false;

    UINT32 maxRingUuid;
    CHECK_RC(FindRing(0, NULL, &maxRingUuid));

    XhciTrbRing * pRing;
    for ( UINT32 i = 0; i<=maxRingUuid; i++ )
    {
        FindRing(i, &pRing);    //<--no CHECK_RC
        if ( pRing )
        {
            if ( !isRingExist )
            {
                Printf(Pri, "Ring List: \n");
                isRingExist = true;
            }
            Printf(Pri, "  %u - \t", i);
            pRing->PrintTitle(Pri);
        }
    }
    if ( !isRingExist )
    {
        Printf(Pri, "No Ring exists \n");
    }
    return OK;
}

RC XhciController::PrintRing(UINT32 Uuid, Tee::Priority Pri)
{
    RC rc;
    XhciTrbRing * pRing;
    CHECK_RC(FindRing(Uuid, &pRing));
    if ( pRing )
    {
        CHECK_RC(pRing->PrintInfo(Pri));
    }
    else
    {
        Printf(Pri, "  Ring %u not exists \n", Uuid);
    }
    return OK;
}

RC XhciController::ClearRing(UINT32 Uuid)
{
    RC rc;

    XhciTrbRing * pRing;
    CHECK_RC(FindRing(Uuid, &pRing));
    if ( pRing )
    {
        CHECK_RC(pRing->Clear());
    }
    else
    {
        Printf(Tee::PriError, "  Ring %u not found \n", Uuid);
        return RC::BAD_PARAMETER;
    }

    return OK;
}

// ----------------------------------------------------------------------------
// Processes
// ----------------------------------------------------------------------------
// this is the implementation of section 4.3 of xHCI spec 0.90
// the whole implementation is scattered to
// ResetPort()
// EnableSlot()
// CmdAddressDev()
// ReqGetDescriptor()
// SetEp0Mps()
// ------------------------ below processed by UsbDevExt & class driver
// ReqGetDescriptor()
// CfgEndpoint(), called by ExtDev
RC XhciController::UsbCfg(UINT32 Port,
                          bool IsLoadClassDriver, /* =true */
                          UINT32 RouteString, /* =0 */
                          UINT32 UsbSpeed /* =0 */)
{
    LOG_ENT();
    Printf(Tee::PriDebug,"(Port %d)\n", Port);
    RC rc;
    UINT08 speed = UsbSpeed;

    if ( 0 != RouteString )
    {   // make sure all HUBs on the RouteString has been configured, if not, configure them one by one
        UINT32 mask = 0xf0000;      // USB spec allows 5 hub tiers at most
        UINT32 tierToEnum;
        for (tierToEnum = 0; tierToEnum < 5; tierToEnum++)
        {
            if (RouteString & (mask >> (tierToEnum*4)))
                break;
        }
        tierToEnum = 5 - tierToEnum;    // number of HUB device we should enum in order to do the last one
        Printf(Tee::PriLow, "  Need to enumerate %d tier HUB\n", tierToEnum);
        for (UINT32 i = 0; i < tierToEnum; i++)
        {
            UINT32 routeString = RouteString & ((1<<(i*4))-1);
            UINT08 hubSlotId;
            CHECK_RC(FindHub(Port, routeString, &hubSlotId));
            if ( 0 == hubSlotId)
            {   // find a HUB not yet enumerated, do configuration
                if ( ( 0 == UsbSpeed ) || ( i != 0 ) )
                {
                    CHECK_RC(ResetPort(Port, routeString, &speed, false));
                }
                Tasker::Sleep(100);
                CHECK_RC(CfgDev(Port, true, speed, routeString));
                Tasker::Sleep(500);
            }
        }
    }
    if ( RouteString || ( 0 == UsbSpeed ) )
    {
        CHECK_RC(ResetPort(Port, RouteString, &speed, false));
    }
    Tasker::Sleep(100);
    CHECK_RC(CfgDev(Port, IsLoadClassDriver, speed, RouteString));
    Tasker::Sleep(100);

    LOG_EXT();
    return OK;
}

RC XhciController::SetupDeviceContext(UINT32 SlotId, UINT32 NumEntries)
{
    RC rc;
    // delete the old Device Context if exists
    DeviceContext * pDummy;
    CHECK_RC(m_pDevContextArray->GetEntry(SlotId, &pDummy, true));
    if (pDummy)
    {
        CHECK_RC(m_pDevContextArray->DeleteEntry(SlotId));
    }
    // setup a blank device context
    UINT32 addrBit;
    CHECK_RC(GetRndAddrBits(&addrBit));
    DeviceContext * pOutpContext = new DeviceContext(m_Is64ByteContext);
    CHECK_RC(pOutpContext->Init(NumEntries, addrBit));
    CHECK_RC(m_pDevContextArray->SetEntry(SlotId, pOutpContext));
    return rc;
}
// configurate the USB device with address 0
// IsLegacy to provide compatibility with legacy USB devices which require
// their Device Descriptor to be read before receiving a SET_ADDRESS request.
RC XhciController::CfgDev(UINT32 Port, bool IsLoadClassDriver, UINT32 Speed,
                          UINT32 RouteString, bool IsLegacy)
{
    LOG_ENT();
    RC rc;
    UINT08 speed = Speed;
    UINT08 usbAddr;
    UINT32 mps;
    void * va;
    // step1: obtain a device slot
    UINT08 slotId;
    // todo: get the SlotType of the port and pass it to IssueEnableSlot()

    CHECK_RC( m_pCmdRing->IssueEnableSlot(&slotId) );
    Printf(Tee::PriNormal, "  SlotId %d assigned by HC \n", slotId);
    m_NewSlotId = slotId;
    if ( !(XHCI_DEBUG_MODE_FAST_TRANS & GetDebugMode()) )
    {
        Printf(Tee::PriLow, "  Slow enum after Enable Slot \n");
        Tasker::Sleep(200);
    }

    // step 2: set the output context (Device context)
    CHECK_RC(SetupDeviceContext(slotId));
    if ( !(XHCI_DEBUG_MODE_FAST_TRANS & GetDebugMode()) )
    {
        Printf(Tee::PriLow, "  Slow enum after Setup Device Context \n");
        Tasker::Sleep(200);
    }

    if (IsLegacy)
    {
        // step2.1: assign NULL USB address to enable control EP
        CHECK_RC( CmdAddressDev(Port, RouteString, speed, slotId, &usbAddr, true) );

        // step2.2: read the dummy devices descriptor
        // allocate MEM to hold the returned Device Descriptor
        CHECK_RC( MemoryTracker::AllocBuffer(1024, &va, true, 32, Memory::UC) );
        // send request Get Device Descriptor to get the MPS
        rc = ReqGetDescriptor(slotId, UsbDescriptorDevice, 0, va, 8);
        if ( OK == rc )
        {
            Memory::Print08(va, 8, Tee::PriLow);
            rc = UsbDevExt::ParseDevDescriptor((UINT08*)va, NULL, &mps);
        }
        MemoryTracker::FreeBuffer(va);
        CHECK_RC(rc);
    }
    // step3: assign USB address
    rc = CmdAddressDev(Port, RouteString, speed, slotId, &usbAddr);
    if ( RC::UNSUPPORTED_FUNCTION_BY_EXT_DEV == rc )
    {
        // Implement A USB Transaction Error Completion Code for an Address Device Command may be due to a Stall
        // response from a device. Software should issue a Disable Slot Command for the Device Slot then
        // an Enable Slot Command to recover from this error
        CHECK_RC( m_pCmdRing->IssueDisableSlot(slotId) );
        CHECK_RC( m_pCmdRing->IssueEnableSlot(&slotId) );

        Printf(Tee::PriNormal, " Address Device failed, retry on Slot %d... \n", slotId);
        CHECK_RC(ResetPort(Port, RouteString, &speed, true));
        Tasker::Sleep(100);
        CHECK_RC( CmdAddressDev(Port, RouteString, speed, slotId, &usbAddr) );
    }
    else
    {
        CHECK_RC(rc);
    }

    Printf(Tee::PriNormal, "  USB address %d assigned by HC \n", usbAddr);
    if ( !(XHCI_DEBUG_MODE_FAST_TRANS & GetDebugMode()) )
    {
        Printf(Tee::PriLow, "  Slow enum after Address Device \n");
        Tasker::Sleep(200);
    }

    // save the usb address <-> slotId match, this also preserved in Slot Context by HW
    //CHECK_RC( SetSlotId(slotId, usbAddr) );

    if (!IsLegacy)
    {
        // step4: read the devices descriptor
        // allocate MEM to hold the returned Device Descriptor
        CHECK_RC( MemoryTracker::AllocBuffer(1024, &va, true, 32, Memory::UC) );
        // send request Get Device Descriptor to get the MPS
        rc = ReqGetDescriptor(slotId, UsbDescriptorDevice, 0, va, 8);
        if ( OK == rc )
        {
            Memory::Print08(va, 8, Tee::PriLow);
            rc = UsbDevExt::ParseDevDescriptor((UINT08*)va, NULL, &mps);
        }
        MemoryTracker::FreeBuffer(va);
        CHECK_RC(rc);
    }

    // step 5: update Device Context for Mps
    CHECK_RC(SetEp0Mps(slotId, mps));

    //  step 6: call factory to create USB class driver
    if ( IsLoadClassDriver )
    {
        UsbDevExt *pUsbDev;
        CHECK_RC( UsbDevExt::Create(this, slotId, speed, RouteString, &pUsbDev) );

        // save the external USB device
        if ( m_pUsbDevExt[slotId] )
        {
            Printf(Tee::PriError, "slot %d already has associated USB device \n", slotId);
            delete pUsbDev;
            return RC::SOFTWARE_ERROR;
        }
        m_pUsbDevExt[slotId] = pUsbDev;
    }

    LOG_EXT();
    return OK;
}

// send Disable Slot Command, release the resource, for best effort, don't use CHECK_RC
RC XhciController::DeCfg(UINT08 SlotId)
{
    LOG_ENT();
    Printf(Tee::PriDebug,"(SlotId %d)", SlotId);

    //DeviceContext * pDevContext;
    //CHECK_RC(m_pDevContextArray->GetEntry(SlotId, &pDevContext));
    //UINT08 port;
    //CHECK_RC(pDevContext->GetRootHubPort(&port));

    // issue the Disable Slot Command
    CmdDisableSlot(SlotId);

    // release the  resources
    // include: Device Context, Xfer Rings, Stream Context Array
    if ( m_pUsbDevExt[SlotId] )
    {
        delete m_pUsbDevExt[SlotId];
        m_pUsbDevExt[SlotId] = NULL;

       // delete Device Context Array entry
        m_pDevContextArray->DeleteEntry(SlotId);
    }

    // delete the Xfer Rings / Stream Context associated
    RemoveRingObj(SlotId, XHCI_DEL_MODE_SLOT);
    RemoveStreamContextArray(SlotId, true);

    LOG_EXT();
    return OK;
}

// search DevContext Array for route match
RC XhciController::GetExtDev(UINT08 RootHubPort, UINT32 RouteString, UsbDevExt ** pExtDev)
{
    RC rc;
    if ( !pExtDev )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }

    DeviceContext * pDevContext;
    UINT08 rootHubPort;
    UINT32 routeString;

    bool isFound = false;
    UINT32 slotId = 1;
    for ( ; slotId < m_MaxSlots; slotId++ )
    {
        CHECK_RC(m_pDevContextArray->GetEntry(slotId, &pDevContext, true));
        if ( pDevContext )
        {
            CHECK_RC(pDevContext->GetRootHubPort(&rootHubPort));
            CHECK_RC(pDevContext->GetRouteString(&routeString));
            if ( ( routeString == RouteString )
                 && ( rootHubPort == RootHubPort ) )
            {
                isFound = true;
                break;
            }
        }
    }
    if ( isFound )
    {
        if ( NULL == m_pUsbDevExt[slotId] )
        {
            Printf(Tee::PriError, "No handle found for external USB device %u \n", slotId);
            return RC::SOFTWARE_ERROR;
        }
        *pExtDev = m_pUsbDevExt[slotId];
    }
    else
    {
        *pExtDev = NULL;
    }
    return OK;
}

// deal with m_vpRings
RC XhciController::RemoveRingObj(UINT08 SlotId, UINT08 DelMode, UINT08 EpNum, bool IsDataOut, UINT16 StreamId)
{
    RC rc;
    // delete the Xfer Rings accociated
    vector<UINT32> vRingIds;
    for ( UINT32 i = 0; i < m_vpRings.size(); i++ )
    {
        if ( m_vpRings[i] )
        {
            if ( XHCI_DEL_MODE_PRECISE != DelMode )
            {
                if ( m_vpRings[i]->IsThatMe(SlotId) )
                {
                    if ( XHCI_DEL_MODE_SLOT_EXPECT_EP0 == DelMode  )
                    {
                        UINT08 dummySlotId;
                        UINT08 epNum;
                        bool dummyIsDataOut;
                        CHECK_RC(m_vpRings[i]->GetHost(&dummySlotId, &epNum, &dummyIsDataOut));
                        if ( 0 != epNum )
                        {
                            delete m_vpRings[i];
                            vRingIds.push_back(i);
                        }
                    }
                    else
                    {
                        delete m_vpRings[i];
                        vRingIds.push_back(i);
                    }
                }
            }
            else
            {
                if ( m_vpRings[i]->IsThatMe(SlotId, EpNum, IsDataOut, StreamId) )
                {
                    delete m_vpRings[i];
                    vRingIds.push_back(i);
                }
            }
        }
        else
        {
            Printf(Tee::PriError, "NUll pointer in Ring Pointer vector \n");
            return RC::SOFTWARE_ERROR;
        }
    }

    reverse(vRingIds.begin(), vRingIds.end());
    for ( UINT32 i = 0; i < vRingIds.size(); i++ )
    {
        m_vpRings.erase(m_vpRings.begin() + vRingIds[i]); // remove it from vector from top to bottom
    }
    return OK;
}

RC XhciController::RemoveStreamContextArray(UINT08 SlotId, bool IsSlotIdOnly, UINT08 EpNum, bool IsDataOut)
{
    // delete the Stream Context Array accociated with specified slot or endpoint
    vector<UINT32> vContextIds;
    for ( UINT32 i = 0; i < m_vpStreamArray.size(); i++ )
    {
        if ( m_vpStreamArray[i] )
        {
            if ( IsSlotIdOnly )
            {
                if ( m_vpStreamArray[i]->IsThatMe(SlotId) )
                {
                    delete m_vpStreamArray[i];
                    vContextIds.push_back(i);
                }
            }
            else
            {
                if ( m_vpStreamArray[i]->IsThatMe(SlotId, EpNum, IsDataOut) )
                {
                    delete m_vpStreamArray[i];
                    vContextIds.push_back(i);
                }
            }
        }
    }

    reverse(vContextIds.begin(), vContextIds.end());
    for ( UINT32 i = 0; i < vContextIds.size(); i++ )
    {
        m_vpStreamArray.erase(m_vpStreamArray.begin() + vContextIds[i]); // remove it from vector from top to bottom
    }
    return OK;
}

RC XhciController::ClearStatusChangeBits(UINT08 Port, UINT32 PortSc)
{
// System software shall acknowledge status change(s) by clearing the respective PORTSC status change bit(s).
// see also Figure 39: Example Port Change Bit Port Status Change Event Generation
    RC rc;
    if ( 0 == PortSc )
    {   // by default clear all
        CHECK_RC( RegRd(XHCI_REG_PORTSC(Port), &PortSc) );
    }
    PortSc = FLD_SET_RF_NUM(XHCI_REG_PORTSC_PED, 0, PortSc);
    PortSc = FLD_SET_RF_NUM(XHCI_REG_PORTSC_PR, 0, PortSc);
    CHECK_RC(RegWr(XHCI_REG_PORTSC(Port), PortSc));
    return rc;
}

RC XhciController::StateSave()
{   // implementation of 4.23.2 xHCI Power Management
    // 1. Stop all USB activity by issuing Stop Endpoint Commands for each endpoint in the Running state.
    RC rc;
    vector<UINT08> vSlots;
    m_pDevContextArray->GetActiveSlots(&vSlots);

    DeviceContext * pDevContext;
    vector<UINT08> vEndpoints;
    for ( UINT32 i = 0; i < vSlots.size(); i++ )
    {
        vEndpoints.clear();
        CHECK_RC(m_pDevContextArray->GetEntry(vSlots[i], &pDevContext));
        CHECK_RC(pDevContext->GetRunningEps(&vEndpoints));

        // save the ep info to the vRunningEndpoint
        for ( UINT32 j = 0; j < vEndpoints.size(); j++ )
        {
            m_vRunningEp.push_back((vSlots[i] << 8) | vEndpoints[j]);
            Printf(Tee::PriLow, " send Stop Endpoint Cmd to slot %d, ep %d \n", vSlots[i], vEndpoints[j]);
            CHECK_RC(CmdStopEndpoint(vSlots[i], vEndpoints[j], false));
        }
    }

    //2) Stop the controller by setting Run/Stop (R/S) = '0'.
    CHECK_RC(StartStop(false));

    //3) Read the USBCMD, DNCTRL, CRCR, DCBAAP, and CONFIG Operational registers and the
    // IMAN, IMOD, ERSTSZ, ERSTBA, and ERDP Runtime Registers and save their state.
    CHECK_RC( MemoryTracker::AllocBuffer(XHCI_REG_RANGE, (void**)&m_pRegBuffer, true, 32, Memory::UC) );
    Memory::Fill08(m_pRegBuffer, 0, XHCI_REG_RANGE);
    // save all instead of individual
    for ( UINT32 i = 0; i < XHCI_REG_RANGE / 4; i++ )
    {
        MEM_WR32( ((UINT32 *)m_pRegBuffer) + i , MEM_RD32(((UINT32 *)m_pRegisterBase) + i) );
    }

    //4) Set the Controller Save State (CSS) flag in the USBCMD register (5.4.1) and wait for the Save
    //State Status (SSS) flag in the USBSTS register (5.4.2) to transition to '0'.
    UINT32 val;
    CHECK_RC(RegRd(XHCI_REG_OPR_USBCMD, &val));
    val = FLD_SET_RF_NUM(XHCI_REG_OPR_USBCMD_CSS, 1, val);
    CHECK_RC(RegWr(XHCI_REG_OPR_USBCMD, val));

    UINT32 unExp = 1 << (0?XHCI_REG_OPR_USBSTS_SSS);
    rc = WaitRegBitClearAny(XHCI_REG_OPR_USBSTS, unExp);
    if ( OK != rc )
    {
        Printf(Tee::PriError, "Wait for SSS timeout! \n");
        return rc;
    }

    //Printf(Tee::PriNormal, " Stop & Saved! \n");
    return OK;
}

RC XhciController::StateRestore()
{   // implementation of 4.23.2 xHCI Power Management
    //2) Restore the Operational and Runtime Registers defined above with their previously saved state.
    RC rc;
    CHECK_RC(SetLocalReg(true, true));
    UINT32 val32;
    for ( UINT32 i = 0; i < m_MaxIntrs; i++ )
    {
        RegRd(XHCI_REG_IMAN(i), &val32);    RegWr(XHCI_REG_IMAN(i), val32);
        RegRd(XHCI_REG_IMOD(i), &val32);    RegWr(XHCI_REG_IMOD(i), val32);
        RegRd(XHCI_REG_ERSTSZ(i), &val32);  RegWr(XHCI_REG_ERSTSZ(i), val32);
        RegRd(XHCI_REG_ERSTBA(i), &val32);  RegWr(XHCI_REG_ERSTBA(i), val32);
        RegRd(XHCI_REG_ERSTBA1(i), &val32); RegWr(XHCI_REG_ERSTBA1(i), val32);
        RegRd(XHCI_REG_ERDP(i), &val32);    RegWr(XHCI_REG_ERDP(i), val32);
        RegRd(XHCI_REG_ERDP1(i), &val32);   RegWr(XHCI_REG_ERDP1(i), val32);
    }
    RegRd(XHCI_REG_OPR_CONFIG, &val32);     RegWr(XHCI_REG_OPR_CONFIG, val32);
    RegRd(XHCI_REG_OPR_DCBAAP_LO, &val32);  RegWr(XHCI_REG_OPR_DCBAAP_LO, val32);
    RegRd(XHCI_REG_OPR_DCBAAP_HI, &val32);  RegWr(XHCI_REG_OPR_DCBAAP_HI, val32);
    CHECK_RC(CmdRingReloc());
    //RegRd(XHCI_REG_OPR_CRCR, &val32);       RegWr(XHCI_REG_OPR_CRCR, val32);  // Pointer in CRCR always read 0
    //RegRd(XHCI_REG_OPR_CRCR1, &val32);      RegWr(XHCI_REG_OPR_CRCR1, val32);
    RegRd(XHCI_REG_OPR_DNCTRL, &val32);     RegWr(XHCI_REG_OPR_DNCTRL, val32);
    RegRd(XHCI_REG_OPR_USBCMD, &val32);     RegWr(XHCI_REG_OPR_USBCMD, val32);
    // very important!
    CHECK_RC(SetLocalReg(false, true));

    MemoryTracker::FreeBuffer(m_pRegBuffer);
    m_pRegBuffer = NULL;

    //3) Set the Controller Restore State (CRS) flag in the USBCMD register (5.4.1) to '1' and wait for the
    //Restore State Status (RSS) flag in the USBSTS register (5.4.2) to transition to '0'.
    CHECK_RC(RegRd(XHCI_REG_OPR_USBCMD, &val32));
    val32 = FLD_SET_RF_NUM(XHCI_REG_OPR_USBCMD_CRS, 1, val32);
    CHECK_RC(RegWr(XHCI_REG_OPR_USBCMD, val32));

    UINT32 unExp = 1 << (0?XHCI_REG_OPR_USBSTS_RSS);
    rc = WaitRegBitClearAny(XHCI_REG_OPR_USBSTS, unExp);
    if ( OK != rc )
    {
        Printf(Tee::PriError, "Wait for RSS timeout! \n");
        return rc;
    }

    //4) Enable the controller by setting Run/Stop (R/S) = '1'.
    CHECK_RC(StartStop(true));

    //5) Software shall walk the USB topology and initialize each of the xHC PORTSC, PORTPMSC, and
    // PORTLI registers, and external hub ports attached to USB devices.
    // seems nothing need to be done at this point

    //6) Restart each of the previously Running endpoints by ringing their doorbells.
    // Note: On NEC controller, ringng doorbell won't bring the stopped endpoint to running state.
    // instead a XferNoOp will do here.
    for( UINT32 i = 0; i < m_vRunningEp.size(); i++ )
    {
        UINT08 epNum;
        bool isOut;
        CHECK_RC(DeviceContext::DciToEp(m_vRunningEp[i] & 0xff, &epNum, &isOut));
        CHECK_RC(RingDoorBell(m_vRunningEp[i]>>8, epNum, isOut));
        Printf(Tee::PriLow, " Ring Doorbell for slot %d, ep %d \n", m_vRunningEp[i]>>8, m_vRunningEp[i] & 0xff);
    }
    return OK;
}

// interface to manipulate m_IsFromLocal
RC XhciController::SetLocalReg(bool IsEnable, bool IsSilent)
{
    m_IsFromLocal = IsEnable;

    if ( m_IsFromLocal )
    {
        if ( NULL == m_pRegBuffer )
        {
            Printf(Tee::PriError, "No local buffer exists for preserving Register \n");
            m_IsFromLocal = false;
            return RC::BAD_PARAMETER;
        }
        else
        {
            if ( !IsSilent )
            {
                Printf(Tee::PriNormal, "Xhci.RegRd() reads from local buffer");
            }
        }
    }
    else
    {
        if ( !IsSilent )
        {
            Printf(Tee::PriNormal, "Xhci.RegRd() reads from HW");
        }
    }
    return OK;
}

RC XhciController::SetEnumMode(UINT32 ModeMask)
{
    m_EnumMode = ModeMask;
    Printf(Tee::PriLow, "Enum Mode 0x%x \n", m_EnumMode);
    Printf(Tee::PriLow, "  Insert CmdSense %s \n",
           (XHCI_ENUM_MODE_INSERT_SENSE & m_EnumMode) ? "T":"F");
    return OK;
}

UINT32 XhciController::GetEnumMode()
{
    return m_EnumMode;
}

RC XhciController::WaitRegBitClearAny(UINT32 Offset, UINT32 UnExp)
{   // copied from DeviceRegRd()
    if ( !m_pRegisterBase )
    {
        Printf(Tee::PriError, "BAR not initialized \n");
        return RC::WAS_NOT_INITIALIZED;
    }

    if ( Offset & XHCI_REG_TYPE_OPERATIONAL )
    {
        Offset &= ~XHCI_REG_TYPE_OPERATIONAL;
        Offset += m_CapLength;
    }
    else if ( Offset & XHCI_REG_TYPE_DOORBELL )
    {
        Offset &= ~XHCI_REG_TYPE_DOORBELL;
        Offset += m_DoorBellOffset;
    }
    else if ( Offset & XHCI_REG_TYPE_RUNTIME )
    {
        Offset &= ~XHCI_REG_TYPE_RUNTIME;
        Offset += m_RuntimeOffset;
    }

    return WaitRegBitClearAnyMem(Offset, UnExp, GetTimeout());
}

RC XhciController::ResetPort(UINT32 Port, UINT32 RouteSting, UINT08 * pSpeed, bool IsForce)
{   // reset a specified port. no matter it's on root hub or USB HUB
    LOG_ENT();
    Printf(Tee::PriDebug,"(Port %d, RouteString 0x%x)", Port, RouteSting);
    RC rc;
    if ( ( Port < 1 ) || ( Port > m_MaxPorts ) )
    {
        Printf(Tee::PriError, "Invalid Port Number %d, valid [1 - %d] \n", Port, m_MaxPorts);
        return RC::BAD_PARAMETER;
    }
    UINT08 speed;
    UINT08 portId;      // for info printing. could be HUB port or Root HUB port number

    if ( RouteSting )
    {   // reset through HUBs
        UINT08 hubSlotId;
        // find its parent HUB and port number
        UINT32 routeString;
        UINT08 port;
        CHECK_RC(ParseRouteSting(RouteSting, &routeString, &port));
        portId = port;
        CHECK_RC(FindHub(Port, routeString, &hubSlotId));
        if ( 0 == hubSlotId )
        {
            Printf(Tee::PriError, "No HUB on port %d with routeString 0x%x found! \n", Port, routeString);
            return RC::BAD_PARAMETER;
        }

        UsbHubDev * pHubDev = (UsbHubDev *) m_pUsbDevExt[hubSlotId];
        CHECK_RC(pHubDev->ResetHubPort(port, &speed));
        if ( pSpeed )
            *pSpeed = speed;
    }
    else
    {   // reset Root HUB port
        m_pEventRings[0]->FlushRing();      // flush the Event Ring 0
        UINT32 portSc;
        bool isToReset = true;

        //step 0: power on the port
        CHECK_RC( RegRd(XHCI_REG_PORTSC(Port), &portSc) );
        Printf(Tee::PriDebug, "  Reg PortSC for Port %d (0x%x): 0x%08x \n", Port, XHCI_REG_PORTSC(Port)& ~XHCI_REG_TYPE_OPERATIONAL, portSc);
        portSc &= ~XHCI_REG_PORTSC_WORKING_MASK;
        portSc = FLD_SET_RF_NUM(XHCI_REG_PORTSC_PP, 0x1,  portSc);
        CHECK_RC( RegWr(XHCI_REG_PORTSC(Port), portSc) );
        // ack the change status bits if any
        CHECK_RC(ClearStatusChangeBits(Port));

#ifdef SIM_BUILD
        // first wait for CCS get set , if no Device Connected, we don't reset port
        Printf(Tee::PriNormal, "Start to wait for CCS bit set... ");

        RegRd(XHCI_REG_PORTSC(Port), & portSc);
        while(FLD_TEST_RF_NUM(XHCI_REG_PORTSC_CCS, 0x0, portSc))
        {   // infinite looop until CCS is set by HW
            RegRd(XHCI_REG_PORTSC(Port), & portSc);
        }
        Printf(Tee::PriNormal, "Done \n");
#endif

        // Reset the port
        XhciTrb eventTrb;
        portId = Port;
        CHECK_RC(RegRd(XHCI_REG_PORTSC(Port), &portSc));
        if ( FLD_TEST_RF_NUM(XHCI_REG_PORTSC_PED, 0x1, portSc) && (!IsForce) )
        {
            // this is likely to be USB3 protocol port, it automatically enable the port
            CHECK_RC( RegRd(XHCI_REG_PORTSC(Port), &portSc) );
            speed = RF_VAL(XHCI_REG_PORTSC_PORT_SPEED, portSc);
            if ( speed >= XhciUsbSpeedSuperSpeed )
            {
                isToReset = false;
                Printf(Tee::PriLow, "Port is enabled by HC for SuperSpeed device, skip the reset.. \n");
            }
        }

        if ( isToReset )
        {
            CHECK_RC(ClearStatusChangeBits(Port));

            CHECK_RC( RegRd(XHCI_REG_PORTSC(Port), &portSc) );
            portSc &= ~XHCI_REG_PORTSC_WORKING_MASK;
            portSc = FLD_SET_RF_NUM(XHCI_REG_PORTSC_PR, 0x1, portSc);

            CHECK_RC(RegWr(XHCI_REG_PORTSC(Port), portSc));
            Printf(Tee::PriDebug, "  Write PortSC with 0x%08x, ", portSc);

            CHECK_RC( RegRd(XHCI_REG_PORTSC(Port), &portSc) );
            Printf(Tee::PriDebug, "  Read PortSC for Port %d: 0x%08x \n", Port, portSc);
            //wait for the event with PRC set.
            do
            {
                do
                {
                    UINT32 startMs = Platform::GetTimeMS();
                    UINT32 elapseMs = 0;
                    do
                    {
                        HandleFwMsg();
                        rc.Clear();     // Restore RC to OK;
                        rc = WaitForEvent(0, XhciTrbTypeEvPortStatusChg, 0, &eventTrb, false);
                        elapseMs = Platform::GetTimeMS() - startMs;
                    } while ( elapseMs < GetTimeout() && (OK != rc) );
                    //rc = WaitForEvent(0, XhciTrbTypeEvPortStatusChg, GetTimeout(), &eventTrb, false);
                    if ( OK != rc )
                    {
                        Printf(Tee::PriError, "ResetPort(%d): Wait for Port Status Change Event Timeout \n", Port);
                        RegRd(XHCI_REG_PORTSC(Port), &portSc);
                        Printf(Tee::PriError, "  Reg PortSC for Port %d: 0x%08x ", Port, portSc);
                        if ( FLD_TEST_RF_NUM(XHCI_REG_PORTSC_PRC, 0, portSc) )
                        { Printf(Tee::PriError, "Port Reset Change = 0, expect 1 "); }
                        if ( FLD_TEST_RF_NUM(XHCI_REG_PORTSC_PED, 0, portSc) )
                        { Printf(Tee::PriError, "Port not enabled by HC"); }
                        Printf(Tee::PriError, " \n");
                        return rc;
                    }
                    CHECK_RC(TrbHelper::ParsePortStatusChgEvent(&eventTrb, NULL, &portId));
                    Printf(Tee::PriDebug, "Port Status Change Event came from Port %d\n", portId);
                } while ( portId != Port );

                CHECK_RC( RegRd(XHCI_REG_PORTSC(Port), &portSc) );
                Printf(Tee::PriDebug, "  Reg PortSC for Port %d(0x%x): 0x%08x \n", Port, XHCI_REG_PORTSC(Port), portSc);
            }
            while( FLD_TEST_RF_NUM(XHCI_REG_PORTSC_PRC, 0, portSc)
                   || FLD_TEST_RF_NUM(XHCI_REG_PORTSC_PED, 0, portSc) );

            CHECK_RC(ClearStatusChangeBits(Port, portSc));
        }

        // get the port speed
        CHECK_RC( RegRd(XHCI_REG_PORTSC(Port), &portSc) );
        speed = RF_VAL(XHCI_REG_PORTSC_PORT_SPEED, portSc);
        Printf(Tee::PriLow, "  Speed of attached device: %d \n", speed);
        if ( pSpeed )
            *pSpeed = speed;
    }

    switch ( speed )
    {
    case XhciUsbSpeedUndefined:
        Printf(Tee::PriNormal, "  Undefined");break;
    case XhciUsbSpeedFullSpeed:
        Printf(Tee::PriNormal, "  Full Speed");break;
    case XhciUsbSpeedLowSpeed:
        Printf(Tee::PriNormal, "  Low Speed");break;
    case XhciUsbSpeedHighSpeed:
        Printf(Tee::PriNormal, "  High Speed");break;
    case XhciUsbSpeedSuperSpeed:
        Printf(Tee::PriNormal, "  Super Speed");break;
    case XhciUsbSpeedSuperSpeedGen2:
        Printf(Tee::PriNormal, "  SS GEN2");break;
    default:
        Printf(Tee::PriNormal, "  Unknown Speed");
    }
    Printf(Tee::PriNormal, " Device connected on Port %d\n", portId);

    LOG_EXT();
    return OK;
}

RC XhciController::FindHub(UINT08 Port, UINT32 RouteString, UINT08 * pSlotId)
{
    LOG_ENT();
    RC rc;
    if (!pSlotId)
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    *pSlotId = 0;

    // go thrugh all existing device
    for ( UINT32 i = 1; i <= XHCI_MAX_SLOT_NUM; i++ )
    {
        if (  m_pUsbDevExt[i] )
        {
            bool isHub;
            UINT08 port;
            UINT32 routeString;
            DeviceContext * pDevCopntext;
            CHECK_RC(m_pDevContextArray->GetEntry(i, &pDevCopntext));
            CHECK_RC(pDevCopntext->GetHub(&isHub));
            if (isHub)
            {
                CHECK_RC(pDevCopntext->GetRootHubPort(&port));
                CHECK_RC(pDevCopntext->GetRouteString(&routeString));
                if ( (port == Port ) && ( routeString == RouteString ) )
                {   // found
                    *pSlotId = i;
                    Printf(Tee::PriLow, "USB HUB @ %d, 0x%x found in Slot %u\n", Port, RouteString, i);
                    break;
                }
            }
        }
    }

    if (*pSlotId == 0)
    {
        Printf(Tee::PriLow, "USB HUB @Port %d, RouteSting 0x%x not found\n", Port, RouteString);
    }
    LOG_EXT();
    return OK;
}

RC XhciController::ParseRouteSting(UINT32 RouteSting, UINT32 * pParentRouteString, UINT08 * pHubPort)
{
    LOG_ENT();
    if (!pParentRouteString || !pHubPort)
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    * pParentRouteString = 0;
    * pHubPort = 0;

    if (RouteSting >= (1<<20) )
    {
        Printf(Tee::PriError, "Invalid route string 0x%08x \n", RouteSting);
        return RC::BAD_PARAMETER;
    }

    UINT32 tierMask = 0xf0000;
    for (UINT32 i = 0; i < 5; i++)
    {
        if (RouteSting & tierMask)
        {   // found the lowest tier, remove it then we get the parent
            * pParentRouteString = RouteSting & (~tierMask);
            * pHubPort = (RouteSting & tierMask) >> 4*(5-1-i);
            break;
        }
        tierMask >>= 4;
    }

    LOG_EXT();
    return OK;
}

RC XhciController::SetEp0Mps(UINT08 SlotId, UINT32 Mps)
{
    LOG_ENT();
    Printf(Tee::PriDebug,"(SlotId %d, Mps %d) \n", SlotId, Mps);
    RC rc;

    // build up input context for Eval Context command
    // SA 6.2.3.3 Evaluate Context Command Usage
    InputContext inpCntxt(m_Is64ByteContext);
    CHECK_RC(inpCntxt.Init());
    CHECK_RC(inpCntxt.SetInuputControl(0, 0x2));    // adding A1
    CHECK_RC(inpCntxt.SetEpMps(0, true, Mps));

    // issue Eval context command
    CHECK_RC(m_pCmdRing->IssueEvalContext(&inpCntxt, SlotId));
    LOG_EXT();
    return OK;
}

RC XhciController::SetDevContextMiscBit(UINT08 SlotId, UINT16 IntrTarget, UINT16 MaxLatMs)
{
    LOG_ENT();
    RC rc;

    // sa 4.5.2 Slot Context Initialization
    //A 'valid' Input Slot Context for an Evaluate Context Command requires the
    // Interrupter Target and Max Exit Latency fields to be initialized.
    InputContext inpCntxt(m_Is64ByteContext);
    CHECK_RC(inpCntxt.Init());
    CHECK_RC(inpCntxt.SetInuputControl(0, 0x1));    // adding A0
    CHECK_RC(inpCntxt.SetIntrTarget(IntrTarget));
    CHECK_RC(inpCntxt.SetMaxExitLatency(MaxLatMs));

    // issue Eval context command
    CHECK_RC(m_pCmdRing->IssueEvalContext(&inpCntxt, SlotId));
    LOG_EXT();
    return OK;
}

RC XhciController::CfgEndpoint(UINT08 SlotId,
                               vector<EndpointInfo>* pvEpInfo,
                               bool IsHub,
                               UINT32 Ttt,
                               bool Mtt,
                               UINT32 NumPorts)
{
// refer to section 6.2.2.2 for the Slot Context fields used by the Configure Endpoint Command
// See also 4.5.2 Slot Context Initialization for requirement on Slot Context
    LOG_ENT();
    RC rc = OK;

    InputContext inpCntxt(m_Is64ByteContext);
    CHECK_RC(inpCntxt.Init());

    UINT08 maxDci = 0;
    UINT32 addMask = 1;    //A0 should be set to '1' by 4.6.6 Configure Endpoint
    if ( IsHub )
    {
    //4.5.2 Slot Context Initialization
    //After entering the Addressed state for the first time from the Enabled or Default states, the values
    //of the Output Slot Context hub related fields (Hub, TTT, MTT, and Number of Ports) shall be
    //initialized by the xHC by the first Configure Endpoint Command to transition the Slot from the
    //Addressed to the Configured state.
    // SA 6.2.2.2 Configure Endpoint Command Usage
        CHECK_RC(inpCntxt.SetHubBit(true));
        CHECK_RC(inpCntxt.SetTtt(Ttt));
        CHECK_RC(inpCntxt.SetMtt(Mtt));
        CHECK_RC(inpCntxt.SetNumOfPorts(NumPorts));
    }

    UINT32 oldRingSize = pvEpInfo->size();      // record for later comparison
    Printf(Tee::PriLow, "  Setup InputContext for for %d Endpoints\n", (UINT32)pvEpInfo->size());
    for ( UINT32 i = 0; i< pvEpInfo->size(); i++ )
    {
        UINT08 epNum = (*pvEpInfo)[i].EpNum;
        bool isDataOut = (*pvEpInfo)[i].IsDataOut;
        UINT08 dci;
        CHECK_RC(DeviceContext::EpToDci(epNum, isDataOut, &dci));
        addMask |= 1 << dci;
        if ( dci < 2 )
        {
            Printf(Tee::PriError, "Shouln't be any control Endpoint in Input Context \n");
            return RC::BAD_PARAMETER;
        }
        if ( dci > maxDci )
            maxDci = dci;

        if ( (*pvEpInfo)[i].IsSsCompanionAvailable
             && (*pvEpInfo)[i].MaxStreams )
        {   // Stream!
            Printf(Tee::PriLow, "Stream is enabled for Endpoint %d, %s\n", epNum, isDataOut?"Out":"In");
            PHYSADDR pPhyBase;
            CHECK_RC(InitStream(SlotId, epNum, isDataOut, &pPhyBase, (*pvEpInfo)[i].MaxStreams));

            CHECK_RC(inpCntxt.SetEpDeqPtr(epNum, isDataOut, pPhyBase));
            CHECK_RC(inpCntxt.InitEpContext(epNum, isDataOut,
                                            (*pvEpInfo)[i].Type,
                                            (*pvEpInfo)[i].Interval,
                                            (*pvEpInfo)[i].MaxBurst,
                                            (*pvEpInfo)[i].MaxStreams,
                                            (*pvEpInfo)[i].Mult,
                                            m_CErr, 0, m_IsNss));
        }
        else
        {
            TransferRing * pXferRing = new TransferRing(SlotId, epNum, isDataOut);
            UINT32 addrBit;
            CHECK_RC(GetRndAddrBits(&addrBit));
            CHECK_RC(pXferRing->Init(m_XferRingSize, addrBit));
            // save the new Xfer Ring
            m_vpRings.push_back(pXferRing);

            PHYSADDR pPhyBase;
            CHECK_RC(pXferRing->GetBaseAddr(&pPhyBase));
            CHECK_RC(inpCntxt.SetEpDeqPtr(epNum, isDataOut, pPhyBase));

            bool isDcs;
            pXferRing->GetPcs(&isDcs);
            CHECK_RC(inpCntxt.SetEpDcs(epNum, isDataOut, isDcs));

            CHECK_RC(inpCntxt.InitEpContext(epNum, isDataOut,
                                            (*pvEpInfo)[i].Type,
                                            (*pvEpInfo)[i].Interval,
                                            (*pvEpInfo)[i].MaxBurst,
                                            (*pvEpInfo)[i].MaxStreams,
                                            (*pvEpInfo)[i].Mult,
                                            m_CErr));
        }

        CHECK_RC(inpCntxt.SetEpMps(epNum, isDataOut, (*pvEpInfo)[i].Mps));
        CHECK_RC(inpCntxt.SetEpBwParams(epNum,
                                        isDataOut,
                                        (*pvEpInfo)[i].Mps,
                                        (*pvEpInfo)[i].IsSsCompanionAvailable?
                                        (*pvEpInfo)[i].BytePerInterval:
                                        (*pvEpInfo)[i].Mps * ((*pvEpInfo)[i].TransPerMFrame + 1)));
    }
    // todo: add drop flags, enum all the slot contexts and find those are not empty
    CHECK_RC(inpCntxt.SetInuputControl(0, addMask));
    CHECK_RC(inpCntxt.SetContextEntries(maxDci));

    rc = CmdCfgEndpoint(SlotId, & inpCntxt);

    if ( OK != rc )
    {   //release the Rings allocated if not success
        inpCntxt.PrintInfo(Tee::PriNormal, Tee::PriNormal); // for debug!!!

        UINT32 numToRemove = m_vpRings.size() - oldRingSize;
        UINT32 index = m_vpRings.size()-1;
        while ( numToRemove )
        {
            delete m_vpRings[index];
            m_vpRings.pop_back();
            numToRemove--;
            index--;
        }
    }

    LOG_EXT();
    return rc;
}

RC XhciController::GetEpMps(UINT08 SlotId, UINT08 Endpoint, bool IsDataOut, UINT32 * pMps)
{
    LOG_ENT();

    RC rc = OK;
    // get device context
    DeviceContext * pDevContext;
    CHECK_RC(m_pDevContextArray->GetEntry(SlotId, &pDevContext));
    // get mps
    CHECK_RC(pDevContext->GetMps(Endpoint, IsDataOut, pMps));

    LOG_EXT();
    return OK;
}

RC XhciController::GetEpState(UINT08 SlotId, UINT08 Endpoint, bool IsDataOut, UINT08 * pEpState)
{
    LOG_ENT();

    RC rc = OK;
    // get device context
    DeviceContext * pDevContext;
    CHECK_RC(m_pDevContextArray->GetEntry(SlotId, &pDevContext));
    // get mps
    CHECK_RC(pDevContext->GetEpState(Endpoint, IsDataOut, pEpState));

    LOG_EXT();
    return OK;
}

// ----------------------------------------------------------------------------
// CMD functions
// Note: This is the set of functions which issue command and need special handling
// for those don't need any special handling, just use m_pCmdRing->IssueXXXX() instead

// Issue a NoOp command and return the current Deq Pointer
RC XhciController::CmdNoOp(XhciTrb ** ppDeqPointer /* = NULL */)
{
    RC rc;
    if ( !m_pCmdRing )
    {
        Printf(Tee::PriError, "CMD Ring doesn't exist \n");
        return RC::SOFTWARE_ERROR;
    }

    XhciTrb * pEnqPtr;
    PHYSADDR pPaDeqPtr;

    m_pEventRings[0]->FlushRing();
    // issue the command but don't wait for completion
    CHECK_RC(m_pCmdRing->IssueNoOp(&pEnqPtr, true, 0));

    // wait for the command completion event
    XhciTrb eventTrb;
    CHECK_RC(WaitForEvent(0, XhciTrbTypeEvCmdComplete, GetTimeout(), &eventTrb));

    // parse the event TRB
    UINT08 cmpCode;
    CHECK_RC(TrbHelper::ParseCmdCompleteEvent(&eventTrb, &cmpCode, &pPaDeqPtr));
    if ( XhciTrbCmpCodeSuccess != cmpCode )
    {
        Printf(Tee::PriError, "Error oclwrs in No Op command, Completion Code = %d \n", cmpCode);
        return RC::HW_STATUS_ERROR;
    }
    else if ( pPaDeqPtr != Platform::VirtualToPhysical(pEnqPtr) )
    {
        Printf(Tee::PriError, "CmdNoOp: Enq 0x%llx and Deq 0x%llx mismatch \n", Platform::VirtualToPhysical(pEnqPtr), pPaDeqPtr);
        return RC::SOFTWARE_ERROR;
    }
    else
        Printf(Tee::PriNormal, "No Op TRB @ 0x%llx(virt:%p) finished. \n", pPaDeqPtr, pEnqPtr);

    if ( NULL != ppDeqPointer )
    {
        * ppDeqPointer = (XhciTrb*)MemoryTracker::PhysicalToVirtual(pPaDeqPtr);
    }
    return OK;
}

// Issue a No Op TRB from the given Xfer Ring, return the current Deq Pointer
RC XhciController::XferNoOp(UINT32 Uuid, XhciTrb ** ppDeqPointer)
{
    LOG_ENT();
    RC rc;

    TransferRing * pXferRing;
    XhciTrbRing * pRing;
    CHECK_RC(FindRing(Uuid, &pRing));
    if ( !pRing->IsXferRing() )
    {
        Printf(Tee::PriError, "Ring %u is not Xfer Ring \n", Uuid);
        return RC::BAD_PARAMETER;
    }
    pXferRing = (TransferRing *) pRing;
    UINT32 intrTarget;
    CHECK_RC(GetIntrTarget(Uuid, &intrTarget));

    CHECK_RC(pXferRing->InsertNoOpTrb());
    XhciTrb * pTrb;
    CHECK_RC(pXferRing->GetBackwardEntry(1, &pTrb));

    UINT08 slotId, endpoint;
    bool isDataOut;
    UINT16 streamId;
    CHECK_RC(pXferRing->GetHost(&slotId, &endpoint, &isDataOut, &streamId));
    CHECK_RC(RingDoorBell(slotId, endpoint, isDataOut, streamId));

    XhciTrb eventTrb;
    CHECK_RC(WaitForEvent(intrTarget, pTrb, 1000, &eventTrb));
    UINT08 completionCode;
    PHYSADDR pPaDeqPtr;
    CHECK_RC(TrbHelper::ParseXferEvent(&eventTrb, &completionCode, &pPaDeqPtr));
    Printf(Tee::PriDebug, "Completion Code = %d", completionCode);

    if ( XhciTrbCmpCodeSuccess != completionCode )
    {
        Printf(Tee::PriError, "Error oclwrs in No Op command, Completion Code = %d \n", completionCode);
        return RC::HW_STATUS_ERROR;
    }
    else
        Printf(Tee::PriNormal, "No Op Xfer @ 0x%llx(virt:%p) finished. \n", pPaDeqPtr, MemoryTracker::PhysicalToVirtual(pPaDeqPtr));

    if ( NULL != ppDeqPointer )
    {
        * ppDeqPointer = (XhciTrb*)MemoryTracker::PhysicalToVirtual(pPaDeqPtr);
    }

    LOG_EXT();
    return OK;
}

UINT32 XhciController::GetOptimisedFragLength()
{
    return TrbHelper::GetMaxXferSizeNormalTrb();
}

RC XhciController::GetNewSlot(UINT32 * pSlotId)
{
    if ( !pSlotId )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    if ( m_NewSlotId == 0 )
    {
        *pSlotId = 0;
        Printf(Tee::PriError, "No Slot ID newly assigned or Slot ID has been retrieved \n");
        return RC::BAD_PARAMETER;
    }
    *pSlotId = m_NewSlotId;
    m_NewSlotId = 0;
    return OK;
}

RC XhciController::GetDevSlots(vector<UINT08> *pvSlots)
{
    if ( pvSlots )
    {
        pvSlots->clear();
    }
    for ( UINT32 i = 1; i <= XHCI_MAX_SLOT_NUM; i++ )
    {
        if ( m_pUsbDevExt[i] != NULL )
        {
            if ( pvSlots )
            {
                pvSlots->push_back(i);
            }
            else
            {
                Printf(Tee::PriNormal, " %d", i);
            }
        }
    }
    if ( !pvSlots )
    {
        Printf(Tee::PriNormal, " \n");
    }
    return OK;
}

RC XhciController::CmdRingReloc()
{
    RC rc = OK;
    UINT32 reg32;
    CHECK_RC( RegRd(XHCI_REG_OPR_CRCR, &reg32) );
    if ( FLD_TEST_RF_NUM(XHCI_REG_OPR_CRCR_CCR, 0x1, reg32) )
    {
        Printf(Tee::PriError, "Command Ring is still running, need stop/abort \n");
        return RC::HW_STATUS_ERROR;
    }

    PHYSADDR pOld;
    PHYSADDR pNew;
    if ( m_pCmdRing )
    {
        m_pCmdRing->GetBaseAddr(&pOld);
        delete m_pCmdRing;
        m_pCmdRing = NULL;
    }

    CHECK_RC(InitCmdRing());
    m_pCmdRing->GetBaseAddr(&pNew);

    Printf(Tee::PriNormal,
           "Cmd Ring Base updated. 0x%llx --> 0x%llx \n",
           pOld, pNew);

    return OK;
}

RC XhciController::CommandStopAbort(bool IsStop, bool IsWaitForEvent)
{   // implementation of 4.6.1.1 Stopping the Command Ring
    RC rc = OK;
    UINT32 reg32;
    CHECK_RC( RegRd(XHCI_REG_OPR_CRCR, &reg32) );
    if ( IsStop )
    {   // stop
        reg32 = FLD_SET_RF_NUM(XHCI_REG_OPR_CRCR_CS, 0x1, reg32);
    }
    else
    {   // abort
        reg32 = FLD_SET_RF_NUM(XHCI_REG_OPR_CRCR_CA, 0x1, reg32);
    }
    CHECK_RC( RegWr(XHCI_REG_OPR_CRCR, reg32) );

    if ( IsWaitForEvent )
    {
        UINT08 compCode;
        do
        {
            XhciTrb eventTrb;
            rc = WaitForEvent(0, XhciTrbTypeEvCmdComplete, GetTimeout(), &eventTrb, false);
            if ( OK != rc )
            {
                break;
            }
            CHECK_RC(TrbHelper::ParseCmdCompleteEvent(&eventTrb, &compCode));
        } while ( XhciTrbCmpCodeCmdRingStopped != compCode );

        if ( OK != rc )
            Printf(Tee::PriError, "Command Completion Event with Completion Code %u not found \n", XhciTrbCmpCodeCmdRingStopped);
    }
    return rc;
}

// The SET_ADDRESS request for a USB2 device shall be issued to Address '0'.
// The SET_ADDRESS request for a USB3 device shall be issued using the Route String.
// when m_pUserContext != NULL, use lwstomized context instead, Port, RouteString, SPeed will be skipped
RC XhciController::CmdAddressDev(UINT08 Port, UINT32 RouteString, UINT08 Speed, UINT08 SlotId, UINT08 * pUsbAddr, bool IsDummy)
{   //partial implementation of Spec 4.3.3 Device Slot Initialization
    RC rc;

    LOG_ENT();
    Printf(Tee::PriDebug,"(Port %u, Speed %u, SlotId %u)", Port, Speed, SlotId);
    if ( !pUsbAddr )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    * pUsbAddr = 0;
    if ( !m_pCmdRing )
    {
        Printf(Tee::PriError, "CMD Ring doesn't exist \n");
        return RC::SOFTWARE_ERROR;
    }

    // Init the data structures associated with the slot, sa 4.5.2 Slot Context Initialization
    InputContext myInp(m_Is64ByteContext);
    myInp.Init(m_UserContextAlign);
    myInp.SetInuputControl(0x0, 0x3);      // set A0 & A1
    myInp.SetContextEntries(1);
    myInp.SetSpeed(Speed);
    myInp.SetRootHubPort(Port);
    myInp.SetRouteString(RouteString);
    //TT fields

    // create a transfer ring for default control pipe
    // create a Input context for used by Address Device Command
    if ((m_vUserContext.size() == 0) || (m_IsFullCopy == false))
    {   // if users are not going to use lwstomized Input Context or
        // not going to manually create Xfer Ring for defualt EP0, we do that for them
        TransferRing * pXferRing = new TransferRing(SlotId, 0, true);
        UINT32 addrBit;
        CHECK_RC(GetRndAddrBits(&addrBit));
        CHECK_RC(pXferRing->Init(m_XferRingSize, addrBit));
        // save the new Xfer Ring
        m_vpRings.push_back(pXferRing);

        PHYSADDR pPhyBase;
        CHECK_RC(pXferRing->GetBaseAddr(&pPhyBase));
        CHECK_RC(myInp.SetEpDeqPtr(0, true, pPhyBase));

        bool isCycleBit;
        CHECK_RC(pXferRing->GetPcs(&isCycleBit));
        myInp.SetEpDcs(0, true, isCycleBit?1:0);
    }
    // for High Speed flash disk, using MPS 8 here causes HC reject the address command with Completion code 17
    UINT32 mps;
    switch ( Speed )
    {
        case XhciUsbSpeedSuperSpeedGen2:
        case XhciUsbSpeedSuperSpeed:
            mps = 512; break;
        case XhciUsbSpeedLowSpeed:
        case XhciUsbSpeedFullSpeed:
            mps = 8; break;
        case XhciUsbSpeedHighSpeed:
            mps = 64; break;
        default:
            Printf(Tee::PriError, "Invalid USB speed %d \n", Speed);
            return RC::BAD_PARAMETER;
    }
    CHECK_RC_CLEANUP(myInp.SetEpMps(0, true, mps));
    CHECK_RC_CLEANUP(myInp.InitEpContext(0, true, XhciEndpointTypeControl,0,0,0,0,m_CErr));

    if (m_vUserContext.size())
    {
        CHECK_RC_CLEANUP(myInp.FromVector(&m_vUserContext, m_IsFullCopy));
        myInp.PrintInfo(Tee::PriNormal);
    }
    else
    {
        myInp.PrintInfo(Tee::PriLow);
    }

    rc = m_pCmdRing->IssueAddressDev(SlotId, &myInp, true, GetTimeout(), IsDummy);
    if ( rc != OK )
    {   // todo: we should build a function GetLastComplationCode(UINT32 EventRingIndex, UINT08* CompCode)
        // get a transaction error
        rc = RC::UNSUPPORTED_FUNCTION_BY_EXT_DEV;     // this is the key for checking
        CHECK_RC_CLEANUP(rc);
    }

    if (!IsDummy)
    {
        // Get the USB address from associated Device Context
        DeviceContext * pDevContext;
        CHECK_RC_CLEANUP(m_pDevContextArray->GetEntry(SlotId, &pDevContext));
        CHECK_RC_CLEANUP(pDevContext->GetUsbDevAddr(pUsbAddr));
    }
    else
        *pUsbAddr = 0;

    LOG_EXT();
    return OK;

Cleanup:    // when error happens
    RemoveRingObj(SlotId, XHCI_DEL_MODE_PRECISE, 0, true);
    return rc;
}

RC XhciController::CmdCfgEndpoint(UINT08 SlotId, InputContext * pInpCntxt,
                              bool IsDeCfg/* = false*/)
{
    LOG_ENT();
    if ( pInpCntxt )
    {
        Printf(Tee::PriLow, "XhciController::CmdCfgEndpoint(): Input Context for CmdCfgEndpoint: ");
        pInpCntxt->PrintInfo(Tee::PriLow);
    }

    RC rc;
    if ( !m_pCmdRing )
    {
        Printf(Tee::PriError, "CMD Ring doesn't exist \n");
        return RC::SOFTWARE_ERROR;
    }

    InputContext myInp(m_Is64ByteContext);
    if (m_vUserContext.size())
    {
        Printf(Tee::PriLow, "XhciController::CmdCfgEndpoint(): Custom Input Context applied\n");
        CHECK_RC(myInp.Init());
        if(!m_IsFullCopy)
        {   // read the current Device Context into myInp,
            // what we actually need is the TR Dequeue Pointer of each EP
            vector<UINT32> vDevContext;
            CHECK_RC(GetDevContext(SlotId, &vDevContext));
            CHECK_RC(myInp.FromVector(&vDevContext, true, true));
        }
        CHECK_RC(myInp.FromVector(&m_vUserContext, m_IsFullCopy));
        myInp.PrintInfo(Tee::PriNormal);
        CHECK_RC(m_pCmdRing->IssueCfgEndp(&myInp, SlotId, IsDeCfg, true, 0));
    }
    else
    {   // Issue command but don't wait for completion
        //Printf(Tee::PriLow, "XhciController::CmdCfgEndpoint(): Input Context dump: \n");
        //pInpCntxt->PrintInfo(Tee::PriLow);
        CHECK_RC(m_pCmdRing->IssueCfgEndp(pInpCntxt, SlotId, IsDeCfg, true, 0));
    }
    // all command event goes to primary Event Ring
    // check out the command status
    XhciTrb eventTrb;
    CHECK_RC(WaitForEvent(0, XhciTrbTypeEvCmdComplete, GetTimeout(), &eventTrb));
    UINT08 completionCode;
    CHECK_RC(TrbHelper::ParseCmdCompleteEvent(&eventTrb, &completionCode));
    if ( XhciTrbCmpCodeSuccess != completionCode )
    {
        Printf(Tee::PriError, "Config Endpoint Error, completion code: %d \n", completionCode);
        return RC::HW_STATUS_ERROR;;
    }
    LOG_EXT();
    return OK;
}

RC XhciController::CmdEvalContext(UINT08 SlotId)
{   // this function is only for JS interface, C++ should call IssueEvalContext()
    LOG_ENT();
    RC rc;
    if ( !m_pCmdRing )
    {
        Printf(Tee::PriError, "CMD Ring doesn't exist \n");
        return RC::SOFTWARE_ERROR;
    }

    InputContext myInp(m_Is64ByteContext);
    if (m_vUserContext.size())
    {
        CHECK_RC(myInp.Init());
        if(!m_IsFullCopy)
        {   // read the current Device Context into myInp
            vector<UINT32> vDevContext;
            CHECK_RC(GetDevContext(SlotId, &vDevContext));
            CHECK_RC(myInp.FromVector(&vDevContext, true, true));
        }
        CHECK_RC(myInp.FromVector(&m_vUserContext, m_IsFullCopy));
        myInp.PrintInfo(Tee::PriNormal);
        CHECK_RC(m_pCmdRing->IssueEvalContext(&myInp, SlotId));
    }
    else
    {
        Printf(Tee::PriError, "No Input Context is speficied, call Xhci.SetUserContext() \n");
        return RC::BAD_PARAMETER;
    }

    LOG_EXT();
    return OK;
}

RC XhciController::CmdResetEndpoint(UINT08 SlotId, UINT08 EpDci, bool IsTsp)
{
    LOG_ENT();
    RC rc;
    if ( !m_pCmdRing )
    {
        Printf(Tee::PriError, "CMD Ring doesn't exist \n");
        return RC::SOFTWARE_ERROR;
    }
    CHECK_RC(m_pCmdRing->IssueResetEndp(SlotId, EpDci, IsTsp));
    LOG_EXT();
    return OK;
}

RC XhciController::CmdStopEndpoint(UINT08 SlotId, UINT08 EpDci, bool IsSp)
{
    LOG_ENT();
    RC rc;
    if ( !m_pCmdRing )
    {
        Printf(Tee::PriError, "CMD Ring doesn't exist \n");
        return RC::SOFTWARE_ERROR;
    }
    CHECK_RC(m_pCmdRing->IssueStopEndp(SlotId, EpDci, IsSp));
    LOG_EXT();
    return OK;
}

RC XhciController::CmdEnableSlot(UINT08 * pSlotId)
{
    RC rc = OK;
    if ( !m_pCmdRing )
    {
        Printf(Tee::PriError, "CMD Ring doesn't exist \n");
        return RC::SOFTWARE_ERROR;
    }
    *pSlotId = 0;
    CHECK_RC( m_pCmdRing->IssueEnableSlot(pSlotId) );
    Printf(Tee::PriNormal, "SlotId %d assigned by HC \n", *pSlotId);
    return rc;
}

RC XhciController::SetUserContext(vector<UINT32>* pvData, UINT32 Alignment)
{
    RC rc = OK;
    if ( !pvData )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }

    m_vUserContext.clear();
    if (pvData->size() == 0)
    {   // user just wants to clear the costomized context
        return OK;
    }

    for(UINT32 i = 0; i < pvData->size(); i++)
    {
        m_vUserContext.push_back((*pvData)[i]);
    }
    m_UserContextAlign = Alignment;

    if (m_vUserContext.size())
    {
        Printf(Tee::PriNormal, "  Lwstomized InputContext %u DWORD", (UINT32)m_vUserContext.size());
        if (m_UserContextAlign)
        {
            Printf(Tee::PriNormal, ", Alignment %u Byte", m_UserContextAlign);
        }
        Printf(Tee::PriNormal, " \n");
    }
    return rc;
}

RC XhciController::SetStreamLayout(vector<UINT32>* pvData)
{
    RC rc = OK;
    if ( !pvData )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }

    m_vUserStreamLayout.clear();
    if (pvData->size() == 0)
    {   // user just wants to clear the costomized context
        return OK;
    }

    for(UINT32 i = 0; i < pvData->size(); i++)
    {
        m_vUserStreamLayout.push_back((*pvData)[i]);
    }
    return rc;
}

RC XhciController::SetupStreamSecArray(UINT08 SlotId, UINT08 Endpoint, bool IsDataOut, UINT32 PriIndex, UINT32 SecSize)
{
    // NSS protection is in StreamContest itself
    RC rc;

    StreamContextArrayWrapper *pStreamCntxt;
    CHECK_RC(FindStream(SlotId, Endpoint, IsDataOut, &pStreamCntxt));

    CHECK_RC(pStreamCntxt->SetupEntry(PriIndex, SecSize));
    return OK;
}

RC XhciController::CmdDisableSlot(UINT08 SlotId)
{
    if ( !m_pCmdRing )
    {
        Printf(Tee::PriError, "CMD Ring doesn't exist \n");
        return RC::SOFTWARE_ERROR;
    }
    return m_pCmdRing->IssueDisableSlot(SlotId);
}

RC XhciController::CmdGetPortBw(UINT08 DevSpeed, UINT08 HubSlotId, vector<UINT08>* pvData)
{
    RC rc;
    if ( !m_pCmdRing )
    {
        Printf(Tee::PriError, "CMD Ring doesn't exist \n");
        return RC::SOFTWARE_ERROR;
    }
    // allocate memory for Port Bandwidth Context
    UINT32 contextByteSize;
    if (HubSlotId)
    {   // if it's a HUB, we allocate 24 entries directly, which is far more than enough
        contextByteSize = 24;
    }
    else
    {   // round up to 8 byte as mentioned by 6.2.6 Port Bandwidth Context
        contextByteSize = m_MaxPorts + 1;
        contextByteSize += (8 - (contextByteSize % 8)) & 0x7;
    }
    void * pVa;
    CHECK_RC( MemoryTracker::AllocBufferAligned(contextByteSize, (void**)&pVa, 16, 32, Memory::UC) );
    Memory::Fill08(pVa, 0, contextByteSize);

    rc = m_pCmdRing->IssueGetPortBandwidth(pVa, DevSpeed, HubSlotId);
    UINT08 * pData08 = (UINT08*) pVa;
    if (pvData)
    {
        pvData->clear();
        for(UINT32 i = 0; i < contextByteSize; i++)
        {
            pvData->push_back( (UINT08) MEM_RD08(pData08+i) );
        }
    }
    else
    {
        for ( UINT32 i = 0; i < contextByteSize; i++ )
        {
            Printf(Tee::PriNormal, " %2d", MEM_RD08(pData08+i));
        }
        Printf(Tee::PriNormal, " \n");
    }
    MemoryTracker::FreeBuffer(pVa);
    return rc;
}

RC XhciController::CmdResetDevice(UINT08 SlotId)
{
    // reclaim all the resources. i.e. Transfer Rings
    // todo: Add Steam support
    RC rc;
    if ( !m_pCmdRing )
    {
        Printf(Tee::PriError, "CMD Ring doesn't exist \n");
        return RC::SOFTWARE_ERROR;
    }
    // hc disables all endpoints of the slot except for the Default Control Endpoint
    // sw does the coresponding job
    for ( UINT08 i = 1; i < XHCI_MAX_DCI_NUM; i++ )
    {
        UINT08 epNum;
        bool isDataOut;
        CHECK_RC(DeviceContext::DciToEp(i, &epNum, &isDataOut));
        TransferRing * pXferRing;
        FindXferRing(SlotId, epNum, isDataOut, & pXferRing); // <-- no CHECK_RC here
        if ( pXferRing )
            CHECK_RC(RemoveRingObj(SlotId, XHCI_DEL_MODE_PRECISE, epNum, isDataOut));
    }
    // inform the xHC that a USB device has been Reset
    CHECK_RC(m_pCmdRing->IssueResetDev(SlotId));
    return OK;
}

RC XhciController::CmdSetTRDeqPtr(UINT08 SlotId, UINT08 Dci, PHYSADDR pPa, bool Dcs, UINT16 StreamId, UINT08 Sct)
{
    //used to modify the TR Dequeue Pointer and DCS fields of an Endpoint or Stream Context
    RC rc;
    if ( !m_pCmdRing )
    {
        Printf(Tee::PriError, "CMD Ring doesn't exist \n");
        return RC::SOFTWARE_ERROR;
    }
    if ((Dci < 1) || (Dci > 31))
    {
        Printf(Tee::PriError, "Invalid Dci %d, valid[1-31] \n", Dci);
        return RC::BAD_PARAMETER;
    }
    Printf(Tee::PriLow, "CmdSetTRDeqPtr(SlotId %d, Dci %d, PHYADDR 0x%llx, Dsc %s) \n",
           SlotId, Dci, pPa, Dcs?"T":"F");
    CHECK_RC(m_pCmdRing->IssueSetDeqPtr(SlotId, Dci, pPa, Dcs, StreamId, Sct));
    return OK;
}

RC XhciController::CmdForceHeader(UINT32 Lo, UINT32 Mid, UINT32 Hi, UINT08 PacketType, UINT08 PortNum)
{
    if ( !m_pCmdRing )
    {
        Printf(Tee::PriError, "CMD Ring doesn't exist \n");
        return RC::SOFTWARE_ERROR;
    }
    return m_pCmdRing->IssueForceHeader(Lo,Mid,Hi,PacketType,PortNum);
}

RC XhciController::CmdGetExtendedProperty(UINT08 SlotId,
                                          UINT08 EpId,
                                          UINT16 Eci,
                                          UINT08 SubType /* = 0*/,
                                          UINT08* pDataBuffer /*= NULL*/)
{
    RC rc = OK;

    if ( !m_pCmdRing )
    {
        Printf(Tee::PriError, "CMD Ring doesn't exist \n");
        return RC::SOFTWARE_ERROR;
    }
    if ( !m_IsGetSetCap )
    {
        Printf(Tee::PriError, "The xHC does't support GSC\n");
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    CHECK_RC(GetEci());

    if (!(Eci & m_Eci))
    {
        Printf( Tee::PriDebug,
                "Supported extended capability: 0x%x \n"
                "Request extended capability: 0x%x \n",
                m_Eci, Eci);
        if (Eci)
        {
            Printf( Tee::PriError,
                "The xHC doesn't support get this extended capability.\n");
            return RC::UNSUPPORTED_HARDWARE_FEATURE;
        }
        else
        {
            Printf(Tee::PriError, "Invalid ECI: 0x%x\n", Eci);
            return RC::ILWALID_ARGUMENT;
        }

    }

    // Issue Get_Ectended_Property command without waiting compeletion
    CHECK_RC( m_pCmdRing->IssueGetExtndProperty(SlotId,
                                                EpId,
                                                SubType,
                                                Eci,
                                                pDataBuffer) );

    return rc;
}

// ----------------------------------------------------------------------------
// General Normal Data Xfer for Bulk, Interrupt & Isoch
RC XhciController::ReserveTd(UINT08 SlotId,
                             UINT08 Endpoint,
                             bool IsDataOut,
                             UINT32 NumTD)
{
    LOG_ENT();
    RC rc;
    // Get the associated Ring
    TransferRing * pXferRing;
    CHECK_RC(FindXferRing(SlotId, Endpoint, IsDataOut, &pXferRing));
    UINT32 numEmptySlot;
    CHECK_RC(pXferRing->GetEmptySlots(&numEmptySlot));

    if ( numEmptySlot >= NumTD )
    {   // there's enough TRB left
        return OK;
    }
    // enlarge the XferRing
    numEmptySlot = NumTD - numEmptySlot;
    UINT32 i = 0;
    UINT32 maxTrb = XhciTrbRing::GetTrbPerSegment();
    for ( ; i < ( (numEmptySlot + maxTrb - 1) / maxTrb ); i++)
    {
        CHECK_RC(pXferRing->AppendTrbSeg(maxTrb));
    }
    Printf(Tee::PriNormal, "%d segments append to ", i);
    pXferRing->PrintTitle(Tee::PriNormal);

    // do we need to enlerge the Event Ring as well ?

    LOG_EXT();
    return OK;
}

RC XhciController::SetupTd(UINT08 SlotId,
                       UINT08 Endpoint,
                       bool IsDataOut,
                       MemoryFragment::FRAGLIST* pFrags,
                       bool IsIsoch,
                       bool IsDbRing,
                       UINT16 StreamId,
                       void * pDataBuffer)
{   // todo: insert Event Data TRB automatically per section 4.11.7.2. of spec 1.0
    LOG_ENT();
    RC rc;

    // Get the associated Ring
    TransferRing * pXferRing;
    CHECK_RC(FindXferRing(SlotId, Endpoint, IsDataOut, &pXferRing, StreamId));

    UINT32 mps;
    CHECK_RC(GetEpMps(SlotId, Endpoint, IsDataOut, &mps));
    CHECK_RC(pXferRing->InsertTd(pFrags, mps, IsIsoch, pDataBuffer, m_RatioIdt));

    // ring the doorbell
    if ( IsDbRing )
    {
        CHECK_RC(RingDoorBell(SlotId, Endpoint, IsDataOut));
    }

    LOG_EXT();
    return OK;
}

// wait for Xfer Completion Event, exposed to JS interface
RC XhciController::WaitXfer(UINT08 SlotId,
                            UINT08 Endpoint,
                            bool IsDataOut,
                            UINT32 TimeoutMs,
                            UINT32 * pCompCode,
                            bool IsWaitAll,
                            UINT32 * pTransferLength,
                            bool IsSilent)
{
    LOG_ENT();
    RC rc;
    // get the Interrut Target, where the Cmd Complation Event TRB will be.
    UINT32 intrTarget;
    CHECK_RC(GetIntrTarget(SlotId, Endpoint, IsDataOut, &intrTarget));

    XhciTrb eventTrb;
    UINT08 completionCode = 0;  // invalid
    UINT32 transferLength;

    if ( IsWaitAll )
    {   // wait for all the TRBs in specified Xfer Ring to be finished
        TransferRing * pXferRing;
        CHECK_RC(FindXferRing(SlotId, Endpoint, IsDataOut, &pXferRing));

        XhciTrb * pTrb;
        CHECK_RC(pXferRing->GetBackwardEntry(1, &pTrb));
        rc = WaitForEvent(intrTarget, pTrb, TimeoutMs, &eventTrb, false);
        if ( OK != rc )
        {
            if ( !IsSilent )
            {
                Printf(Tee::PriError, "Error on waiting for the last TRB(@0x%llx) from Enpoint %d Data%s to finish \n", Platform::VirtualToPhysical((void*)pTrb), Endpoint, IsDataOut?"Out":"In");
            }
            return rc;
        }
    }
    else
    {   // wait for any Xfer Event of specified Xfer Ring to be finished
        UINT08 dci;
        CHECK_RC(DeviceContext::EpToDci(Endpoint, IsDataOut, &dci));
        rc = WaitForEvent(intrTarget, XhciTrbTypeEvTransfer, TimeoutMs, &eventTrb, false, SlotId, dci);
        if ( OK != rc )
        {
            if( !IsSilent )
            {
                Printf(Tee::PriError, "Error on waiting for Xfer Event for any TRB from Enpoint %d Data%s \n", Endpoint, IsDataOut?"Out":"In");
            }
            return rc;
        }
    }
    // parse the completion code
    CHECK_RC(TrbHelper::ParseXferEvent(&eventTrb, &completionCode, NULL, &transferLength));

    if ( XhciTrbCmpCodeStallErr == completionCode
         || XhciTrbCmpCodeBabbleDetectErr == completionCode)
    {   // STALL or Babble, we need to reset the endpoint
        ResetEndpoint(SlotId, Endpoint, IsDataOut);
        Printf(Tee::PriNormal, "  Reset EP SlotId %d, Ep %d, Data %s \n",
               SlotId, Endpoint, IsDataOut?"Out":"In");
    }
    else if ( XhciTrbCmpCodeUsbTransactionErr == completionCode
              && m_IsBto)
    {
        m_BusErrCount++;
        ResetEndpoint(SlotId, Endpoint, IsDataOut);
        Printf(Tee::PriDebug, "  Reset EP SlotId %d, Ep %d, Data %s \n",
               SlotId, Endpoint, IsDataOut?"Out":"In");
    }

    if ( pCompCode )
    {
        *pCompCode = completionCode;
    }
    else
    {
        CHECK_RC(ValidateCompCode(completionCode));
    }

    if ( pTransferLength )
    {
        *pTransferLength = transferLength;
    }

    Printf(Tee::PriLow, "Xfer Completion Code = %d \n", completionCode);
    LOG_EXT();
    return OK;
}

RC XhciController::ValidateCompCode(UINT32 CompletionCode, bool *pIsStall)
{
    if ( pIsStall )
    {
        *pIsStall = false;
        if (CompletionCode == XhciTrbCmpCodeStallErr)
        {
            Printf(Tee::PriLow, "  STALL detected \n");
            *pIsStall = true;
        }
    }

    if ( (CompletionCode != XhciTrbCmpCodeSuccess)
         && (CompletionCode != XhciTrbCmpCodeShortPacket)
         && (CompletionCode != XhciTrbCmpCodeStallErr)
         && ((!m_IsBto) || (CompletionCode != XhciTrbCmpCodeUsbTransactionErr))
        )
    {
        Printf(Tee::PriError, "Bad Completion Code %d \n", CompletionCode);
        return RC::HW_STATUS_ERROR;
    }
    return OK;
}

RC XhciController::ResetEndpoint(UINT08 SlotId, UINT08 Endpoint, bool IsDataOut)
{   // implementation of 4.6.8 Reset Endpoint
    LOG_ENT();
    RC rc;
    // check if Endpoint Context is set to Halted
    DeviceContext * pDevContext;
    CHECK_RC(m_pDevContextArray->GetEntry(SlotId, &pDevContext));
    UINT08 epState;
    CHECK_RC(pDevContext->GetEpState(Endpoint,  IsDataOut, &epState));
    if ( epState != 2 ) // todo: define enum for endpoint state
    {
        Printf(Tee::PriWarn, "ResetEndpoint(Slot %d, Ep %d, Data%s) Ep state: %d, not in Halt, skipping.. \n",
               SlotId, Endpoint, IsDataOut?"Out":"In", epState);
        return OK;
    }
    Printf(Tee::PriLow, "ResetEndpoint(Slot %d, Ep %d, Data%s)  Ep state: %d \n",
           SlotId, Endpoint, IsDataOut?"Out":"In", epState);

    UINT08 dci;
    DeviceContext::EpToDci(Endpoint, IsDataOut, &dci);
    CHECK_RC(CmdResetEndpoint(SlotId, dci, false));

    // check if the state has become STOP
    CHECK_RC(pDevContext->GetEpState(Endpoint,  IsDataOut, &epState));
    if ( epState != 3 )
    {
        Printf(Tee::PriError, "  Ep state: %d, should be 3 (STOPPED)  \n", epState);
        return RC::HW_STATUS_ERROR;
    }

    // issue the CLEAR FEATURE to clear the stall ffrom device. reset the data toggle / sequence number
    if ( 0 != Endpoint )
    {
        CHECK_RC(ReqClearFeature(SlotId, 0, UsbRequestEndpoint, Endpoint | (IsDataOut?0:0x80)));
    }

    // reset the transfer ring, retire all un-finished TDs
    CHECK_RC(CleanupTd(SlotId, Endpoint, IsDataOut, false));

    Tasker::Sleep(500);
    Printf(Tee::PriLow, "  ResetEndpoint() Done \n");
    LOG_EXT();
    return OK;
}

RC XhciController::WaitCmdCompletion(UINT08* pCmpCode, UINT08* pSlotId, UINT32 TimeoutMs)
{
    LOG_ENT();
    RC rc;
    XhciTrb * pCmdTrb;
    XhciTrb eventTrb;
    CHECK_RC(m_pCmdRing->GetBackwardEntry(1, &pCmdTrb));
    rc = WaitForEvent(0, pCmdTrb, TimeoutMs, &eventTrb);
    if ( OK != rc )
    {
        Printf(Tee::PriError, "Timeout on waiting for Command TRB to finish ");
        TrbHelper::DumpTrbInfo(pCmdTrb, true,  Tee::PriNormal, true,  false);
        return rc;
    }

    // do some parse of the returned event TRB
    UINT08 compCode, slotId;
    CHECK_RC(TrbHelper::ParseCmdCompleteEvent(&eventTrb, &compCode, NULL, &slotId));
    if ( pSlotId )
        *pSlotId = slotId;
    if ( pCmpCode )
        *pCmpCode = compCode;
    else
    {
        if ( compCode != XhciTrbCmpCodeSuccess )
        {
            Printf(Tee::PriError, "Bad Command Completion Code: %d \n", compCode);
            return RC::HW_STATUS_ERROR;
        }
    }
    LOG_EXT();
    return OK;
}

// ----------------------------------------------------------------------------
// Standard Device Requests
// this is the implementation of USB 3.0 Spec 9.4
RC XhciController::ReqGetDescriptor(UINT08 SlotId, UsbDescriptorType DescType, UINT08 Index, void * pDataIn, UINT16 Len)
{
    LOG_ENT();
    Printf(Tee::PriLow,"ReqGetDescriptor(SlotId = %d, Type %d)", SlotId, DescType);
    RC rc;

    CHECK_RC(Setup3Stage(SlotId,
                         UsbRequestDevice|UsbRequestDeviceToHost,
                         UsbGetDescriptor,
                         DescType<<8|Index,
                         ((UsbDescriptorType)DescType == UsbDescriptorString)?0x0409:0,  // For English
                         Len,
                         pDataIn));

    LOG_EXT();
    return OK;
}

RC XhciController::ReqSetConfiguration(UINT08 SlotId, UINT16 CfgValue)
{
    Printf(Tee::PriLow,"ReqSetConfiguration(SlotId = %d, Cfg %d)", SlotId, CfgValue);
    return Setup2Stage(SlotId,
                       UsbRequestDevice|UsbRequestHostToDevice,
                       UsbSetConfiguration,
                       CfgValue,
                       0,
                       0);
}

// work together with CmdResetEndpoint
RC XhciController::ReqClearFeature(UINT08 SlotId, UINT16 Feature, UsbRequestRecipient Recipient, UINT16 Index)
{
    Printf(Tee::PriLow,"ReqClearFeature(SlotId = %d, Feature %d)", SlotId, Feature);
    if ( Feature != 0
         && Feature != 48
         && Feature != 49
         && Feature != 50
         && Feature != 1
         && Feature != 2 )
    {   // see also USB3.0 spec Table 9.6 and USB2.0 spec table 9-6
        Printf(Tee::PriError, "Bad Feature Selector %u, valid [0-Ep_Halt/Func_Suspend, 1-DEVICE_REMOTE_WAKEUP, 48-U1_Enable, 49-U2_Enable, 50-LTM_Enable] \n", Feature);
        return RC::SOFTWARE_ERROR;
    }

    return Setup2Stage(SlotId,
                       Recipient|UsbRequestHostToDevice,
                       UsbClearFeature,
                       Feature,
                       Index,
                       0);
}

RC XhciController::ReqGetConfiguration(UINT08 SlotId, UINT08 *pData08)
{
    RC rc;
    Printf(Tee::PriLow,"ReqGetConfiguration(SlotId = %d)", SlotId);
    UINT08 * pDataIn;
    UINT32 addrBit = 32;
    CHECK_RC( MemoryTracker::AllocBuffer(1, (void**)&pDataIn, true, addrBit, Memory::UC) );
    MEM_WR08(pDataIn, 0);

    rc = Setup3Stage(SlotId,
                     UsbRequestDevice|UsbRequestDeviceToHost,
                     UsbGetConfiguration,
                     0,
                     0,
                     1,
                     (void*)pDataIn);
    if ( OK == rc )
    {
        MEM_WR08(pData08, MEM_RD08(pDataIn));
    }
    else
    {
        MEM_WR08(pData08, 0);
    }
    MemoryTracker::FreeBuffer(pDataIn);
    return rc;
}

RC XhciController::ReqGetInterface(UINT08 SlotId, UINT16 Interface, UINT08 *pData08)
{
    RC rc;
    Printf(Tee::PriLow,"ReqGetInterface(SlotId = %d, Interface = %d)", SlotId, Interface);
    UINT08 * pDataIn;
    UINT32 addrBit = 32;
    CHECK_RC( MemoryTracker::AllocBuffer(1, (void**)&pDataIn, true, addrBit, Memory::UC) );
    MEM_WR08(pDataIn, 0);

    rc = Setup3Stage(SlotId,
                     UsbRequestInterface|UsbRequestDeviceToHost,
                     UsbGetInterface,
                     0,
                     Interface,
                     1,
                     (void * )pDataIn);
    if ( OK == rc )
    {
        MEM_WR08(pData08, MEM_RD08(pDataIn));
    }
    else
    {
        MEM_WR08(pData08, 0);
    }
    MemoryTracker::FreeBuffer(pDataIn);
    return rc;
}

RC XhciController::ReqGetStatus(UINT08 SlotId, UsbRequestRecipient Recipient, UINT16 Index, UINT16 *pData16)
{
    RC rc;
    Printf(Tee::PriLow,"ReqGetStatus(SlotId = %d)", SlotId);
    UINT16 * pDataIn;
    UINT32 addrBit = 32;
    CHECK_RC( MemoryTracker::AllocBuffer(2, (void**)&pDataIn, true, addrBit, Memory::UC) );
    MEM_WR16(pDataIn, 0);

    rc = Setup3Stage(SlotId,
                     Recipient|UsbRequestDeviceToHost,
                     UsbGetStatus,
                     0,
                     Index,
                     2,
                     (void * )pDataIn);
    if ( OK == rc )
    {
        MEM_WR16(pData16, MEM_RD16(pDataIn));
    }
    else
    {
        MEM_WR16(pData16, 0);
    }
    MemoryTracker::FreeBuffer(pDataIn);
    return rc;
}

RC XhciController::ReqSetFeature(UINT08 SlotId, UINT16 Feature, UsbRequestRecipient Recipient, UINT08 Index, UINT08 Suspend)
{
    Printf(Tee::PriLow,"ReqSetFeature(SlotId = %d, Feature = %d)", SlotId, Feature);
    if ( Feature != 0
         && Feature != 48
         && Feature != 49
         && Feature != 50
         && Feature != 1
         && Feature != 2 )
    {   // see also USB3.0 spec Table 9.6 and USB2.0 spec table 9-6
        Printf(Tee::PriError, "Bad Feature Selector %u, valid [0-Ep_Halt/Func_Suspend, 1-DEVICE_REMOTE_WAKEUP, 48-U1_Enable, 49-U2_Enable, 50-LTM_Enable] \n", Feature);
        return RC::SOFTWARE_ERROR;
    }

    return Setup2Stage(SlotId,
                       Recipient|UsbRequestHostToDevice,
                       UsbSetFeature,
                       Feature,
                       Index|Suspend<<8,
                       0);
}

RC XhciController::ReqSetInterface(UINT08 SlotId, UINT16 Alternate, UINT16 Interface)
{
    Printf(Tee::PriLow,"ReqSetInterface(SlotId = %d)", SlotId);
    return Setup2Stage(SlotId,
                       UsbRequestInterface|UsbRequestHostToDevice,
                       UsbSetInterface,
                       Alternate,
                       Interface,
                       0);
}

RC XhciController::ReqSetSEL(UINT08 SlotId, UINT08 U1Sel, UINT08 U1Pel, UINT16 U2Sel, UINT16 U2Pel)
{
    RC rc;
    Printf(Tee::PriLow,"ReqSetSEL(SlotId = %d)", SlotId);
    UINT08 * pData;
    UINT32 addrBit = 32;
    CHECK_RC( MemoryTracker::AllocBuffer(6, (void**)&pData, true, addrBit, Memory::UC) );

    MEM_WR08(&pData[0], U1Sel);
    MEM_WR08(&pData[1], U1Pel);
    MEM_WR08(&pData[2], U2Sel);
    U2Sel >>= 8;
    MEM_WR08(&pData[3], U2Sel);
    MEM_WR08(&pData[4], U2Pel);
    U2Pel >>= 8;
    MEM_WR08(&pData[5], U2Pel);

    rc = Setup3Stage(SlotId,
                     UsbRequestDevice|UsbRequestHostToDevice,
                     UsbSetSel,
                     0,
                     0,
                     6,
                     (void*)pData);
    MemoryTracker::FreeBuffer(pData);
    return rc;
}

RC XhciController::ReqMassStorageReset(UINT08 SlotId, UINT16 Interface)
{
    Printf(Tee::PriLow,"ReqMassStorageReset(SlotId = %d, Interface %d)", SlotId, Interface);
    return Setup2Stage(SlotId,
                       UsbRequestClass|UsbRequestInterface|UsbRequestHostToDevice,
                       0xff,
                       0,
                       Interface,
                       0);
}

// Hub requests below
RC XhciController::HubReqClearHubFeature(UINT08 SlotId, UINT16 Feature)
{
    Printf(Tee::PriLow,"HubReqClearHubFeature(SlotId = %d, Feature %d)", SlotId, Feature);
    return Setup2Stage(SlotId,0x20,UsbHubClearFeature,Feature,0,0);
}

RC XhciController::HubReqClearPortFeature(UINT08 SlotId, UINT16 Feature, UINT08 Port, UINT08 IndicatorSelector /*=0*/)
{
    Printf(Tee::PriLow,"HubReqClearPortFeature(SlotId = %d)", SlotId);
    return Setup2Stage(SlotId,0x23,UsbHubClearFeature,Feature,(IndicatorSelector<<8)|Port,0);
}

RC XhciController::HubReqGetHubDescriptor(UINT08 SlotId, void * pDataIn, UINT16 Len, bool IsSuperSpeed)
{   // by default, all hubs are required to implement one hub descriptor, with index zero
    Printf(Tee::PriLow,"HubReqGetHubDescriptor(SlotId = %d)", SlotId);
    if (IsSuperSpeed)
    {
        return Setup3Stage(SlotId,0xa0,UsbHubGetDescriptor, UsbDescriptorHubDescriptorSs<<8,0,Len,pDataIn);
    }
    return Setup3Stage(SlotId,0xa0,UsbHubGetDescriptor, UsbDescriptorHubDescriptor<<8,0,Len,pDataIn);
}

RC XhciController::HubReqGetHubStatus(UINT08 SlotId, UINT32 * pData)
{
    Printf(Tee::PriLow,"HubReqGetHubStatus(SlotId = %d)", SlotId);
    RC rc;
    UINT32 * pDataIn;
    UINT32 addrBit = 32;
    CHECK_RC( MemoryTracker::AllocBuffer(4, (void**)&pDataIn, true, addrBit, Memory::UC) );
    MEM_WR32(pDataIn, 0);

    rc = Setup3Stage(SlotId,0xa0,UsbHubGetStatus,0,0,4,pDataIn);
    if ( OK == rc )
    {
        MEM_WR32(pData, MEM_RD32(pDataIn));
    }
    else
    {
        MEM_WR32(pData, 0);
    }
    MemoryTracker::FreeBuffer(pDataIn);
    return rc;
}

RC XhciController::HubReqPortErrorCount(UINT08 SlotId, UINT16 Port, UINT16 * pData)
{
    Printf(Tee::PriLow,"HubReqPortErrorCount(SlotId = %d, Port %d)", SlotId, Port);
    RC rc;
    UINT16 * pDataIn;
    UINT32 addrBit = 32;
    CHECK_RC( MemoryTracker::AllocBuffer(2, (void**)&pDataIn, true, addrBit, Memory::UC) );
    MEM_WR16(pDataIn, 0);

    rc = Setup3Stage(SlotId,0x80,UsbHubGetPortErrCount,0,Port,2,pDataIn);
    if ( OK == rc )
    {
        MEM_WR16(pData, MEM_RD16(pDataIn));
    }
    else
    {
        MEM_WR16(pData, 0);
    }
    MemoryTracker::FreeBuffer(pDataIn);
    return rc;
}

RC XhciController::HubReqGetPortStatus(UINT08 SlotId, UINT16 Port, UINT32 * pData)
{   // see also USB3.0 spec table 10.10 is want more interpret
    Printf(Tee::PriLow,"HubReqGetPortStatus(SlotId = %d, Port %d)", SlotId, Port);
    RC rc;
    UINT32 * pDataIn;
    UINT32 addrBit = 32;
    CHECK_RC( MemoryTracker::AllocBuffer(4, (void**)&pDataIn, true, addrBit, Memory::UC) );
    MEM_WR32(pDataIn, 0);

    rc = Setup3Stage(SlotId,0xa3,UsbHubGetStatus,0,Port,4,pDataIn);
    if ( OK == rc )
    {
        MEM_WR32(pData, MEM_RD32(pDataIn));
    }
    else
    {
        MEM_WR32(pData, 0);
    }
    MemoryTracker::FreeBuffer(pDataIn);
    return rc;
}

RC XhciController::HubReqSetHubFeature(UINT08 SlotId, UINT16 Feature, UINT16 Index, UINT16 Length)
{
    Printf(Tee::PriLow,"HubReqSetHubFeature(SlotId = %d, Feature %d)", SlotId, Feature);
    return Setup2Stage(SlotId,0x20,UsbHubSetFeature,Feature,Index,Length);
}

RC XhciController::HubReqSetHubDepth(UINT08 SlotId, UINT16 HubDepth)
{
    Printf(Tee::PriLow,"HubReqSetHubDepth(SlotId = %d, HubDepth %d)", SlotId, HubDepth);
    return Setup2Stage(SlotId,0x20,UsbHubSetHubDepth,HubDepth,0,0);
}

RC XhciController::HubReqSetPortFeature(UINT08 SlotId, UINT16 Feature, UINT08 Port, UINT08 Index /*=0*/)
{
    Printf(Tee::PriLow,"HubReqSetPortFeature(SlotId = %d, Feature %d)", SlotId, Feature);
    return Setup2Stage(SlotId,0x23,UsbHubSetFeature,Feature,(Index<<8)|Port,0);
}

// ----------------------------------------------------------------------------
// Request common functions
RC XhciController::Setup3Stage(UINT08 SlotId, UINT08 ReqType, UINT08 Req,
                               UINT16 Value, UINT16 Index, UINT16 Length,
                               void * pData)
{
    LOG_ENT();
    Printf(Tee::PriLow, "Setup3Stage for device @ Slot %d:\n", SlotId);
    Printf(Tee::PriLow, "  ReqType 0x%02x, Req %d, Value 0x%04x, Index 0x%04x, Length 0x%04x\n", ReqType, Req, Value, Index, Length);

    RC rc;

    TransferRing * pXferRing;
    CHECK_RC(FindXferRing(SlotId, 0, true, & pXferRing));

    CHECK_RC(pXferRing->InsertSetupTrb(ReqType, Req, Value, Index, Length));
    CHECK_RC(pXferRing->InsertDataStateTrb(pData, Length, (ReqType & 0x80) == UsbRequestHostToDevice));
    if (DefaultRandom::GetRandom(0, 99) < m_EventData)
    {
        CHECK_RC(pXferRing->InsertEventDataTrb());
    }
    CHECK_RC(pXferRing->InsertStastusTrb(true));
    // Get the address of Status TRB

    CHECK_RC(RingDoorBell(SlotId, 0, true));
    CHECK_RC(WaitXfer(SlotId, 0, true));

    LOG_EXT();
    return OK;
}

RC XhciController::Setup2Stage(UINT08 SlotId, UINT08 ReqType, UINT08 Req, UINT16 Value, UINT16 Index, UINT16 Length)
{
    LOG_ENT();
    Printf(Tee::PriLow, "Setup2Stage for device @ slot %d:\n", SlotId);
    Printf(Tee::PriLow, "  ReqType 0x%02x, Req %d, Value 0x%04x, Index 0x%04x, Length 0x%04x\n", ReqType, Req, Value, Index, Length);

    RC rc;

    TransferRing * pXferRing;
    CHECK_RC(FindXferRing(SlotId, 0, true, & pXferRing));

    CHECK_RC(pXferRing->InsertSetupTrb(ReqType, Req, Value, Index, Length));
    CHECK_RC(pXferRing->InsertStastusTrb(false));

    CHECK_RC(RingDoorBell(SlotId, 0, true));
    CHECK_RC(WaitXfer(SlotId, 0, true));

    LOG_ENT();
    return OK;
}

// ----------------------------------------------------------------------------
// Portal funtions
RC XhciController::GetDevContext(UINT08 SlotId, vector<UINT32>* pvData)
{
    RC rc;
    DeviceContext* pDevContext;
    CHECK_RC(m_pDevContextArray->GetEntry(SlotId, &pDevContext));
    if ( pvData )
    {
        CHECK_RC(pDevContext->ToVector(pvData));
    }
    else
    {
        vector<UINT32> vTmp;
        CHECK_RC(pDevContext->ToVector(&vTmp));
        Printf(Tee::PriNormal, "Device Context of Slot %d: \n", SlotId);
        VectorData::Print(&vTmp);
    }
    return OK;
}

RC XhciController::SetDevContext(UINT08 SlotId, vector<UINT32>* pvData)
{
    RC rc;
    DeviceContext* pDevContext;
    CHECK_RC(m_pDevContextArray->GetEntry(SlotId, &pDevContext));
    CHECK_RC(pDevContext->FromVector(pvData, true));
    return OK;
}

RC XhciController::SetRingSize(UINT08 Type, UINT32 Size)
{
    if ( Size > XhciTrbRing::GetTrbPerSegment() )
    {
        Printf(Tee::PriError, "Invalid size %u, valid [1-%u] \n", Size, XhciTrbRing::GetTrbPerSegment());
        return RC::BAD_PARAMETER;
    }
    if ( XHCI_RING_TYPE_CMD == Type )
        m_CmdRingSize = Size;
    else if ( XHCI_RING_TYPE_EVENT == Type )
        m_EventRingSize = Size;
    else if ( XHCI_RING_TYPE_XFER == Type )
        m_XferRingSize = Size;
    else
    {
        Printf(Tee::PriError, "Wrong Type of Ring %u, valid [%u,%u,%u] \n", Type, XHCI_RING_TYPE_CMD, XHCI_RING_TYPE_EVENT, XHCI_RING_TYPE_XFER);
        return RC::BAD_PARAMETER;
    }
    return OK;
}

RC XhciController::SetCmdRingSize(UINT32 Size)
{
    RC rc;
    CHECK_RC(SetRingSize(XHCI_RING_TYPE_CMD, Size));
    return OK;
}

RC XhciController::SetEventRingSize(UINT32 Size)
{
    RC rc;
    CHECK_RC(SetRingSize(XHCI_RING_TYPE_EVENT, Size));
    return OK;
}

RC XhciController::SetXferRingSize(UINT32 Size)
{
    RC rc;
    CHECK_RC(SetRingSize(XHCI_RING_TYPE_XFER, Size));
    return OK;
}

RC XhciController::GetTrb(UINT32 RingUuid, UINT32 SegIndex, UINT32 TrbIndex, XhciTrb * pTrb)
{
    RC rc;
    if ( !pTrb )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    XhciTrbRing * pRing;
    CHECK_RC(FindRing(RingUuid, &pRing));
    CHECK_RC(pRing->GetTrbEntry(SegIndex, TrbIndex, pTrb));
    return OK;
}

RC XhciController::SetTrb(UINT32 RingUuid, UINT32 SegIndex, UINT32 TrbIndex, XhciTrb * pTrb)
{
    RC rc;
    if ( !pTrb )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    XhciTrbRing * pRing;
    CHECK_RC(FindRing(RingUuid, &pRing));
    CHECK_RC(pRing->SetTrbEntry(SegIndex, TrbIndex, pTrb));

    return OK;
}

RC XhciController::SetRingPtr(UINT32 Uuid, UINT32 SegIndex, UINT32 TrbIndex, INT32 CycleBit)
{
    RC rc;

    XhciTrbRing * pRing;
    CHECK_RC(FindRing(Uuid, &pRing));

    if ( pRing->IsXferRing() )
    {
        TransferRing * pTmp = (TransferRing *) pRing;
        CHECK_RC(pTmp->SetEnqPtr(SegIndex, TrbIndex, CycleBit));
    }
    else if ( pRing->IsCmdRing() )
    {
        CmdRing * pTmp = (CmdRing *) pRing;
        CHECK_RC(pTmp->SetEnqPtr(SegIndex, TrbIndex, CycleBit));
    }
    else if ( pRing->IsEventRing() )
    {
        EventRing * pTmp = (EventRing *) pRing;
        CHECK_RC(pTmp->SetDeqPtr(SegIndex, TrbIndex, CycleBit));
    }
    else
    {
        Printf(Tee::PriError, "Strange Ring Type \n");
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

// Ring scheme:
// 0 : Command Ring
// 1 - maxIntrs : Event Rings
// maxIntrs+1 - : Xfer Rings
RC XhciController::FindRing(UINT32 Uuid, XhciTrbRing ** ppRing, UINT32 * pMaxUuid) const
{
    LOG_ENT();
    Printf(Tee::PriDebug,"(Uuid %u)", Uuid);

    UINT32 maxUuid = m_vpRings.size() + m_MaxIntrs + m_UuidOffset;
    if ( pMaxUuid )
    {
        *pMaxUuid = maxUuid;
        return OK;
    }
    if ( !ppRing )
    {
        Printf(Tee::PriError, "pRing null pointer \n");
        return RC::BAD_PARAMETER;
    }

    if ( m_UuidOffset && (Uuid < m_UuidOffset) )
    {   // if we are going to have more Rings, change here.
        Printf(Tee::PriDebug, "Warning: This UUID is reserved for now");
        *ppRing = NULL;
    }
    else if ( m_UuidOffset == Uuid )
    {
        *ppRing = m_pCmdRing;
    }
    else if ( ( Uuid >=(1 + m_UuidOffset) ) && ( Uuid <= (m_MaxIntrs + m_UuidOffset) ) )
    {
        *ppRing = m_pEventRings[Uuid - 1];
    }
    else
    {
        if ( Uuid > maxUuid )
        {
            Printf(Tee::PriError, "Uuid %u overeflow \n", Uuid);
            *ppRing = NULL;
        }
        else
        {
            *ppRing = m_vpRings[Uuid - m_UuidOffset - m_MaxIntrs - 1];
        }
    }

    LOG_EXT();
    if ( !(*ppRing) )
        return RC::BAD_PARAMETER;   // we don't return NULL without any error

    return OK;
}

RC XhciController::FindCmdRing(UINT32 *pUuid)
{
    if ( !pUuid )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    if ( m_pCmdRing )
    {
        *pUuid = m_UuidOffset;
    }
    else
    {
        Printf(Tee::PriError, "Command Ring doesn't exist \n");
        return RC::BAD_PARAMETER;
    }
    return OK;
}

RC XhciController::FindEventRing(UINT32 EventIndex, UINT32 *pUuid)
{
    if ( !pUuid )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    *pUuid = 0;
    if ( EventIndex >= m_MaxIntrs )
    {
        Printf(Tee::PriError, "Event Ring index %u overflow, valid[0-%u] \n", EventIndex, m_MaxIntrs-1);
        return RC::BAD_PARAMETER;
    }
    if ( m_pEventRings[EventIndex] == NULL )
    {
        Printf(Tee::PriError, "Event Ring index %u not exist \n", EventIndex);
        return RC::BAD_PARAMETER;
    }
    *pUuid = m_UuidOffset + 1 + EventIndex;   // plus one for CMD ring
    return OK;
}

RC XhciController::FindXferRing(UINT08 SlotId, UINT08 EpNum, bool IsDataOut, UINT32 * pUuid, UINT16 StreamId)
{
    if ( !pUuid )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    *pUuid = 0;

    for ( UINT32 i = 0; i<m_vpRings.size(); i++ )
    {
        if ( m_vpRings[i]->IsThatMe(SlotId, EpNum, IsDataOut, StreamId) )
        {
            *pUuid = m_UuidOffset + m_MaxIntrs + i + 1;
            Printf(Tee::PriDebug, "  Xfer Ring ID %u \n", *pUuid);
            return OK;
        }
    }
    Printf(Tee::PriError, "No Xfer Ring defined for Endpoint(%d, %d, Data %s, StreamId %d) \n", SlotId, EpNum, IsDataOut?"Out":"In", StreamId);
    return RC::BAD_PARAMETER;
}

RC XhciController::FindXferRing(UINT08 SlotId, UINT08 EndPoint, bool IsDataOut, TransferRing ** ppRing, UINT16 StreamId)
{
    LOG_ENT();
    Printf(Tee::PriDebug,"(SlotID %d, Endpoint %d, IsDataOut %s)", SlotId, EndPoint, IsDataOut?"T":"F");
    for ( UINT32 i = 0; i<m_vpRings.size(); i++ )
    {
        if ( m_vpRings[i]->IsThatMe(SlotId, EndPoint, IsDataOut, StreamId) )
        {
            *ppRing = m_vpRings[i];
            return OK;
        }
    }

    * ppRing = NULL;
    Printf(Tee::PriError, "No Transfer Ring defined for this Endpoint");
    return RC::BAD_PARAMETER;
}

RC XhciController::FindStream(UINT08 SlotId, UINT08 EndPoint, bool IsDataOut,
                          StreamContextArrayWrapper ** ppStreamCntxt)
{
    LOG_ENT();
    Printf(Tee::PriDebug,"(SlotID %d, Endpoint %d, IsDataOut %s)", SlotId, EndPoint, IsDataOut?"T":"F");
    for ( UINT32 i = 0; i<m_vpStreamArray.size(); i++ )
    {
        if ( m_vpStreamArray[i]->IsThatMe(SlotId, EndPoint, IsDataOut) )
        {
            *ppStreamCntxt = m_vpStreamArray[i];
            return OK;
        }
    }

    * ppStreamCntxt = NULL;
    Printf(Tee::PriError, "No Stream Context Array defined for this Endpoint");
    return RC::BAD_PARAMETER;
}

RC XhciController::RingAppend(UINT32 RingUuid, UINT32 SegSize)
{
    RC rc;
    XhciTrbRing * pRing;
    CHECK_RC(FindRing(RingUuid, &pRing));

    if ( pRing->IsXferRing() )
    {
        TransferRing * pTmp = (TransferRing *) pRing;
        CHECK_RC(pTmp->AppendTrbSeg(SegSize));
    }
    else if ( pRing->IsCmdRing() )
    {
        CmdRing * pTmp = (CmdRing *) pRing;
        CHECK_RC(pTmp->AppendTrbSeg(SegSize));
    }
    else if ( pRing->IsEventRing() )
    {
        EventRing * pTmp = (EventRing *) pRing;
        CHECK_RC(pTmp->AppendTrbSeg(SegSize));
    }
    else
    {
        Printf(Tee::PriError, "Strange Ring Type \n");
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

RC XhciController::RingRemove(UINT32 RingUuid, UINT32 SegIndex)
{
    RC rc;
    XhciTrbRing * pRing;
    CHECK_RC(FindRing(RingUuid, &pRing));

    if ( pRing->IsXferRing() )
    {
        TransferRing * pTmp = (TransferRing *) pRing;
        CHECK_RC(pTmp->RemoveTrbSeg(SegIndex));
    }
    else if ( pRing->IsCmdRing() )
    {
        CmdRing * pTmp = (CmdRing *) pRing;
        CHECK_RC(pTmp->RemoveTrbSeg(SegIndex));
    }
    else if ( pRing->IsEventRing() )
    {
        EventRing * pTmp = (EventRing *) pRing;
        CHECK_RC(pTmp->RemoveTrbSeg());
    }
    else
    {
        Printf(Tee::PriError, "Strange Ring Type \n");
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

RC XhciController::GetRingType(UINT32 RingId, UINT32 * pType)
{
    if ( !pType )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    XhciTrbRing * pRing;
    FindRing(RingId, &pRing);
    if ( !pRing )
    {
        Printf(Tee::PriError, "Ring %u not exist \n", RingId);
        return RC::BAD_PARAMETER;
    }
    if ( pRing->IsCmdRing() )
    {
        * pType = XHCI_RING_TYPE_CMD;
    }
    else if ( pRing->IsEventRing() )
    {
        * pType = XHCI_RING_TYPE_EVENT;
    }
    else if ( pRing->IsXferRing() )
    {
        * pType = XHCI_RING_TYPE_XFER;
    }
    else
    {
        Printf(Tee::PriError, "Bad Ring type \n");
        return RC::ILWALID_INPUT;
    }
    return OK;
}

RC XhciController::GetRingPtr(UINT32 RingUuid, UINT32 * pSeg, UINT32 * pIndex,
                          PHYSADDR * pDeqEnqPtr, UINT32 BackLevel)
{
    RC rc;
    if ( !pSeg || !pIndex )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    XhciTrbRing * pRing;
    CHECK_RC(FindRing(RingUuid, &pRing));

    /*
    if (0 == BackLevel)
    {
        return pRing->GetPtr(pSeg, pIndex);
    }
    */
    XhciTrb * pTmp;
    rc = pRing->GetBackwardEntry(BackLevel, &pTmp, pSeg, pIndex);
    if (pDeqEnqPtr)
    {
        * pDeqEnqPtr = Platform::VirtualToPhysical(pTmp);
    }
    return OK;
}

RC XhciController::GetRingSize(UINT32 RingUuid, vector<UINT32> *pvSizes)
{
    RC rc;
    XhciTrbRing * pRing;
    CHECK_RC(FindRing(RingUuid, &pRing));

    if ( !pvSizes )
    {
        vector<UINT32> vTmp;
        CHECK_RC(pRing->GetSegmentSizes(&vTmp));
        Printf(Tee::PriNormal, "Ring %d segment sizes: ", RingUuid);
        for (UINT32 i = 0; i <vTmp.size(); i++ )
        {
            Printf(Tee::PriNormal, " %d", vTmp[i]);
        }
        Printf(Tee::PriNormal, "\n");
    }
    else
    {
        CHECK_RC(pRing->GetSegmentSizes(pvSizes));
    }
    return OK;
}

RC XhciController::RingDoorBell(UINT32 SlotId, UINT08 Endpoint, bool IsOut, UINT16 StreamId)
{
    LOG_ENT();
    Printf(Tee::PriDebug,"(SlotId %d, EpNum %d, IsOut %s, StreamId %d)", SlotId, Endpoint, IsOut?"T":"F", StreamId);
    RC rc;
    if ( Endpoint > 15 )
    {
        Printf(Tee::PriError, "Invalid Endpoint %d, valid[0-15] \n", Endpoint);
        return RC::BAD_PARAMETER;
    }
    if ( SlotId > m_MaxSlots )
    {
        Printf(Tee::PriError, "Invalid SlotId %d, valid[1-%d] \n", SlotId, m_MaxSlots);
        return RC::BAD_PARAMETER;
    }

    UINT08 target = 0;
    if ( SlotId == 0 )
    {   // Doorbell Register 0 is dedicated to the Host Controller.
        // For this register, there is only one valid value for the DB Target field, 0
        target = 0;
        //This field only applies to Device Context Doorbells and shall be cleared to 0 for Host Controller Commands.
        StreamId = 0;
    }
    else
    {
        CHECK_RC(DeviceContext::EpToDci(Endpoint, IsOut, &target));
    }

    UINT32 data32 = 0;
    data32 = FLD_SET_RF_NUM(XHCI_REG_DOORBELL_TARGET, target, data32);
    data32 = FLD_SET_RF_NUM(XHCI_REG_DOORBELL_STREAM_ID, StreamId, data32);
    CHECK_RC(RegWr(XHCI_REG_DOORBELL(SlotId), data32));

    Printf(Tee::PriDebug,"0x%08x written to DoorBell %d", data32, SlotId);
    LOG_EXT();
    return OK;
}

RC XhciController::FlushEvent(UINT32 EventRingIndex, UINT32 * pNumEvent)
{
    if ( EventRingIndex>=m_MaxIntrs )
    {
        Printf(Tee::PriError, "Invalid interrupt index %d, valid [0-%d] \n", EventRingIndex, m_MaxIntrs-1);
        return RC::BAD_PARAMETER;
    }
    if ( NULL == m_pEventRings[EventRingIndex] )
    {
        Printf(Tee::PriError, "Event Ring %d does not exist \n", EventRingIndex);
        return RC::BAD_PARAMETER;
    }
    return m_pEventRings[EventRingIndex]->FlushRing(pNumEvent);
}

bool XhciController::CheckXferEvent(UINT32 EventRingIndex, UINT08 SlotId, UINT08 Endpoint, bool IsDataOut, XhciTrb * pTrb)
{
    if ( EventRingIndex>=m_MaxIntrs )
    {
        Printf(Tee::PriError, "Invalid interrupt index %d, valid [0-%d] \n", EventRingIndex, m_MaxIntrs-1);
        return false;
    }
    if ( NULL == m_pEventRings[EventRingIndex] )
    {
        Printf(Tee::PriError, "Event Ring %d does not exist \n", EventRingIndex);
        return false;
    }
    UINT08 dci;
    DeviceContext::EpToDci(Endpoint, IsDataOut, &dci);

    return m_pEventRings[EventRingIndex]->CheckXferEvent(SlotId, dci, pTrb);
}

RC XhciController::WaitForEvent(UINT32 EventRingIndex,
                            XhciTrb * pTrb,
                            UINT32 WaitMs,
                            XhciTrb *pEventTrb,
                            bool IsFlushEvents)
{
    if ( EventRingIndex>=m_MaxIntrs )
    {
        Printf(Tee::PriError, "Invalid interrupt index %d, valid [0-%d] \n", EventRingIndex, m_MaxIntrs-1);
        return RC::BAD_PARAMETER;
    }
    return m_pEventRings[EventRingIndex]->WaitForEvent(pTrb, WaitMs, pEventTrb, IsFlushEvents);
}

RC XhciController::WaitForEvent(UINT32 EventRingIndex,
                                UINT08 EventType,
                                UINT32 WaitMs,
                                XhciTrb *pEventTrb,
                                bool IsFlushEvents,
                                UINT08 SlotId,
                                UINT08 Dci)
{
    if ( EventRingIndex>=m_MaxIntrs )
    {
        Printf(Tee::PriError, "Invalid interrupt index %d, valid [0-%d] \n", EventRingIndex, m_MaxIntrs-1);
        return RC::BAD_PARAMETER;
    }
    return m_pEventRings[EventRingIndex]->WaitForEvent(EventType, WaitMs, pEventTrb, IsFlushEvents, SlotId, Dci);
}

RC XhciController::GetPortProtocol(UINT32 Port, UINT16 *pProtocol)
{
    if ( !pProtocol )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    if ( (Port < 1) || (Port > m_MaxPorts) )
    {
        Printf(Tee::PriError, "Invalid Port %u, valid [1 - %u] \n", Port, m_MaxPorts);
        return RC::BAD_PARAMETER;
    }
    if ( m_vPortProtocol.size() == 0 )
    {
        Printf(Tee::PriError, "No port protocol information provided by HC yet \n");
        return RC::ILWALID_INPUT;
    }
    UINT32 i;
    for ( i = 0; i < m_vPortProtocol.size(); i++ )
    {
        UINT32 portUpperLimit = m_vPortProtocol[i].CompPortOffset;
        portUpperLimit += m_vPortProtocol[i].CompPortCount;
        if ( (Port >= m_vPortProtocol[i].CompPortOffset)
             && (Port < portUpperLimit) )
            break;
    }
    if ( i < m_vPortProtocol.size() )
    {   // found!
        *pProtocol = m_vPortProtocol[i].UsbVer;
    }
    else
    {
        *pProtocol = 0;
        Printf(Tee::PriWarn, "No protocol information for Port %u", Port);
    }
    return OK;
}

RC XhciController::ReadPortProtocol()
{   // 7.2 xHCI Supported Protocol Capability, read from xHCI Extended Capabilities
    // address is in m_ExtCapPtr
    if ( !m_ExtCapPtr )
    {
        Printf(Tee::PriLow, "Extended Capability notimplemented\n");
        return OK;
    }

    RC rc;
    Tee::Priority pri = Tee::PriNormal;
    bool isExit = false;
    UINT32 data32;
    Printf(Tee::PriLow, "Extended Capabilities List (start from DWORD 0x%x):\n", m_ExtCapPtr);

    UINT32 capOffset = m_ExtCapPtr;
    UINT08 capId = 0;
    UINT08 nextCap = 0;
    do
    {
        capOffset += nextCap;
        CHECK_RC(RegRd(capOffset * 4, &data32));
        capId = data32 & 0xff;
        nextCap = ( data32 >> 8 ) & 0xff;

        Printf(Tee::PriLow, "Cap found at 0x%x, ID = 0x%02x Next 0x%02x:\n", capOffset, capId, nextCap);

        switch ( capId )
        {
        case XHCI_XCAP_ID_LEG:
            Printf(Tee::PriLow, "  USB Legacy Support\n"); // 8 Byte
            Printf(Tee::PriLow, "    0x%08x  ", data32);
            CHECK_RC(RegRd((capOffset +1 )* 4, &data32));
            Printf(Tee::PriLow, "    0x%08x\n", data32);
            break;
        case XHCI_XCAP_ID_PROTOCOL:
            Printf(Tee::PriLow, "  USB Protocol \n");
            Protocol pro;
            char nameString[9];

            nameString[4] = 0X30 + RF_VAL(XHCI_REG_XCAP_PROTOCOL_REV_MAJOR, data32);
            nameString[5] = 0x2e;
            nameString[6] = 0X30 + (RF_VAL(XHCI_REG_XCAP_PROTOCOL_REV_MINOR, data32) >> 4);
            nameString[7] = 0X30 + (RF_VAL(XHCI_REG_XCAP_PROTOCOL_REV_MINOR, data32) & 0xf);
            nameString[8] = 0x0;

            pro.UsbVer = RF_VAL(XHCI_REG_XCAP_PROTOCOL_REV_MAJOR, data32) << 8;
            pro.UsbVer |= RF_VAL(XHCI_REG_XCAP_PROTOCOL_REV_MINOR, data32);

            RegRd((capOffset + 1)* 4, &data32);   // read name string
            nameString[0] = data32 & 0xff;
            nameString[1] = (data32 >> 8) & 0xff;
            nameString[2] = (data32 >> 16) & 0xff;
            nameString[3] = data32 >> 24;

            RegRd((capOffset + 2)* 4, &data32);   // read caps
            pro.CompPortOffset = RF_VAL(XHCI_REG_XCAP_PROTOCOL_PORT_OFFSET, data32);
            pro.CompPortCount = RF_VAL(XHCI_REG_XCAP_PROTOCOL_PORT_COUNT, data32);

            RegRd((capOffset + 3)* 4, &data32);
            pro.ProtocolSlotType = RF_VAL(XHCI_REG_XCAP_PROTOCOL_SLOT_TYPE, data32);

            Printf(Tee::PriLow,
                   "    %s: Port %d - %d, SlotType %d \n",
                   nameString,
                   pro.CompPortOffset, pro.CompPortOffset + pro.CompPortCount - 1,
                   pro.ProtocolSlotType);

            if ( m_HcVer >= 0x100 )
            {   // only avaliable in Xhci v1.0
                pro.Psic = RF_VAL(XHCI_REG_XCAP_PROTOCOL_PSIC, data32);
                if ( pro.Psic )
                {   // read the Protocol Speed ID one by one
                    for ( UINT32 i = 0; i < pro.Psic; i++ )
                    {
                        RegRd((capOffset + 4 + i)* 4, &(pro.Psi[i]));
                        Printf(Tee::PriLow,
                               "      PSI %d of %d: 0x%08x \n",
                               i, pro.Psic, pro.Psi[i]);
                    }
                }
            }
            else
            {
                pro.Psic = 0;
            }

            m_vPortProtocol.push_back(pro);
            break;
        case XHCI_XCAP_ID_PM:
            Printf(Tee::PriLow, "  USB Power Management\n");
            break;
        case XHCI_XCAP_ID_IO_VIRT:
            Printf(Tee::PriLow, "  USB I/O Virtualization\n");
            break;
        case XHCI_XCAP_ID_DEBUG:
            Printf(Tee::PriLow, "  USB Debug\n");
            break;
        case XHCI_XCAP_ID_MSI:
            Printf(Tee::PriLow, "  USB MSI\n");
            break;
        case XHCI_XCAP_ID_EXT_MSI:
            Printf(Tee::PriLow, "  USB MSI-X\n");
            break;
        default:
            Printf(pri, "Unknown Cap %u\n", capId);
            isExit = true;
            break;
        }
    }
    while ( (nextCap != 0) && !isExit );

    return OK;
}

RC XhciController::UpdateCmdRingDeq(XhciTrb* pTrb, bool IsAdvancePtr)
{
    LOG_ENT();
    if ( !pTrb )
    {
        Printf(Tee::PriError, "Cmd TRB null pointer \n");
        return RC::BAD_PARAMETER;
    }
    RC rc;
    if ( !m_pCmdRing )
    {
        Printf(Tee::PriError, "Command Ring has not been created yet \n");
        return RC::WAS_NOT_INITIALIZED;
    }

    CHECK_RC(m_pCmdRing->UpdateDeqPtr(pTrb, IsAdvancePtr));
    return OK;
}

RC XhciController::UpdateEventRingDeq(UINT32 EventIndex, XhciTrb * pDeqPtr, UINT32 SegmengIndex)
{
    LOG_ENT();
    RC rc;
    PHYSADDR pTrb = Platform::VirtualToPhysical(pDeqPtr);
    Printf(Tee::PriDebug, "Updating Event Ring %d DeqPtr to %p (0x%llx) \n", EventIndex, pDeqPtr, pTrb);

    UINT32 data32 = pTrb;
    data32 = FLD_SET_RF_NUM(XHCI_REG_ERDP_DESI, SegmengIndex, data32);
    // if SW does not touch Event Ring since last time a interrup asserted,
    // no interrupt generated even when IMODC count down to 0 and Event Ring not empty
    data32 = FLD_SET_RF_NUM(XHCI_REG_ERDP_EHB, 0x1, data32);
    CHECK_RC(RegWr(XHCI_REG_ERDP(EventIndex), data32));
    pTrb = pTrb >> 32;
    data32 = pTrb;
    CHECK_RC(RegWr(XHCI_REG_ERDP1(EventIndex), data32));

    LOG_EXT();
    return OK;
}

RC XhciController::GetRndAddrBits(UINT32 *pDmaBits)
{
    RC rc = OK;
    *pDmaBits = 32;
    if (*pDmaBits == 64)
    {   // if user specified -64bit and -ratio64, randomize the return value
        *pDmaBits = (DefaultRandom::GetRandom(0, 99) < m_Ratio64Bit)?64:32;
    }
    return rc;
}

// class driver wrapper --------------------------------------------------------
RC XhciController::CheckDevValid(UINT08 SlotId)
{
    if ( m_pUsbDevExt[SlotId] == NULL )
    {
        Printf(Tee::PriError, "No USB device with slot ID %d, call Xhci.GetDevSlots() for valid slot IDs \n", SlotId);
        return RC::BAD_PARAMETER;
    }
    return OK;
}

RC XhciController::GetDevClass(UINT08 SlotId, UINT32 *pType)
{
    RC rc;
    if ( !pType )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    CHECK_RC(CheckDevValid(SlotId));

    *pType = m_pUsbDevExt[SlotId]->GetDevClass();
    return OK;
}

RC XhciController::GetExtStorageDev(UINT08 SlotId, UsbStorage** ppStor)
{
    RC rc;
    CHECK_RC(CheckDevValid(SlotId));
    if ( m_pUsbDevExt[SlotId]->GetDevClass() != UsbClassMassStorage )
    {
        Printf(Tee::PriError, "Command only supported on Mass Storage devices \n");
        return RC::UNSUPPORTED_FUNCTION_BY_EXT_DEV;
    }
    *ppStor = (UsbStorage*) m_pUsbDevExt[SlotId];
    return OK;
}

RC XhciController::GetSectorSize(UINT08 SlotId, UINT32 * pSectorSize)
{
    RC rc = OK;
    UsbStorage *pStor;
    CHECK_RC(GetExtStorageDev(SlotId, &pStor));
    CHECK_RC(pStor->GetSectorSize(pSectorSize));
    return rc;
}

RC XhciController::GetMaxLba(UINT08 SlotId, UINT32 * pMaxLba)
{
    RC rc = OK;
    UsbStorage *pStor;
    CHECK_RC(GetExtStorageDev(SlotId, &pStor));
    CHECK_RC(pStor->GetMaxLba(pMaxLba));
    return rc;
}

RC  XhciController::CmdRead10(UINT08 SlotId, UINT32 Lba, UINT32 NumSectors,
                              MemoryFragment::FRAGLIST * pFragList,
                              vector<UINT08> *pvCbw)
{
    RC rc = OK;
    UsbStorage *pStor;
    CHECK_RC(GetExtStorageDev(SlotId, &pStor));
    CHECK_RC(pStor->CmdRead10(Lba, NumSectors, pFragList, pvCbw));
    return rc;
}

RC  XhciController::CmdRead16(UINT08 SlotId, UINT32 Lba, UINT32 NumSectors,
                              MemoryFragment::FRAGLIST * pFragList,
                              vector<UINT08> *pvCbw)
{
    RC rc = OK;
    UsbStorage *pStor;
    CHECK_RC(GetExtStorageDev(SlotId, &pStor));
    CHECK_RC(pStor->CmdRead16(Lba, NumSectors, pFragList, pvCbw));
    return rc;
}

RC  XhciController::CmdWrite10(UINT08 SlotId, UINT32 Lba, UINT32 NumSectors,
                               MemoryFragment::FRAGLIST * pFragList,
                               vector<UINT08> *pvCbw)
{
    RC rc = OK;
    UsbStorage *pStor;
    CHECK_RC(GetExtStorageDev(SlotId, &pStor));
    CHECK_RC(pStor->CmdWrite10(Lba, NumSectors, pFragList, pvCbw));
    return rc;
}

RC  XhciController::CmdWrite16(UINT08 SlotId, UINT32 Lba, UINT32 NumSectors,
                               MemoryFragment::FRAGLIST * pFragList,
                               vector<UINT08> *pvCbw)
{
    RC rc = OK;
    UsbStorage *pStor;
    CHECK_RC(GetExtStorageDev(SlotId, &pStor));
    CHECK_RC(pStor->CmdWrite16(Lba, NumSectors, pFragList, pvCbw));
    return rc;
}

RC XhciController::CmdSense(UINT08 SlotId, void * pBuf, UINT08 Len)
{
    RC rc = OK;
    UsbStorage *pStor;
    CHECK_RC(GetExtStorageDev(SlotId, &pStor));
    CHECK_RC(pStor->CmdSense(pBuf, Len));
    return rc;
}

RC XhciController::CmdModeSense(UINT08 SlotId, void * pBuf, UINT08 Len)
{
    RC rc = OK;
    UsbStorage *pStor;
    CHECK_RC(GetExtStorageDev(SlotId, &pStor));
    CHECK_RC(pStor->CmdModeSense(pBuf, Len));
    return rc;
}

RC XhciController::CmdTestUnitReady(UINT08 SlotId)
{
    RC rc = OK;
    UsbStorage *pStor;
    CHECK_RC(GetExtStorageDev(SlotId, &pStor));
    CHECK_RC(pStor->CmdTestUnitReady());
    return rc;
}

RC XhciController::CmdInquiry(UINT08 SlotId, void * pBuf, UINT32 Len)
{
    RC rc = OK;
    UsbStorage *pStor;
    CHECK_RC(GetExtStorageDev(SlotId, &pStor));
    CHECK_RC(pStor->CmdInq(pBuf, Len));
    return rc;
}

RC XhciController::CmdReadFormatCapacity(UINT08 SlotId, UINT32 * pNumBlocks, UINT32 * pSectorSize)
{
    MASSERT((pNumBlocks!=0) && (pSectorSize!=0));
    RC rc = OK;
    UsbStorage *pStor;
    CHECK_RC(GetExtStorageDev(SlotId, &pStor));
    CHECK_RC(pStor->CmdReadFormatCapacity(pNumBlocks, pSectorSize));
    return rc;

}

RC XhciController::CmdReadCapacity(UINT08 SlotId, UINT32 * pMaxLBA, UINT32 * pSectorSize)
{
    MASSERT((pMaxLBA!=0) && (pSectorSize!=0));
    RC rc = OK;
    UsbStorage *pStor;
    CHECK_RC(GetExtStorageDev(SlotId, &pStor));
    CHECK_RC(pStor->CmdReadCapacity(pMaxLBA, pSectorSize));
    return rc;
}

RC XhciController::ResetHc()
{
    RC rc;
    UINT32 reg;
    // need to be in Halt state before reset
    CHECK_RC(StartStop(false));

    RegRd(XHCI_REG_OPR_USBSTS, &reg);
    if ( FLD_TEST_RF_NUM(XHCI_REG_OPR_USBSTS_HCH, 0x1, reg) )
    {
        RegRd(XHCI_REG_OPR_USBCMD, &reg);
        reg = FLD_SET_RF_NUM(XHCI_REG_OPR_USBCMD_HCRST, 0x1, reg);
        RegWr(XHCI_REG_OPR_USBCMD, reg);
        Printf(Tee::PriLow, "HC reset, wait for reset finish\n");
        rc = WaitRegMem(m_CapLength+(XHCI_REG_OPR_USBCMD^XHCI_REG_TYPE_OPERATIONAL), 0, RF_SHIFTMASK(XHCI_REG_OPR_USBCMD_HCRST), GetTimeout());
        if ( OK != rc )
        {
            Printf(Tee::PriError, "Wait for HCRST clear timeout \n");
            return rc;
        }
    }
    else
    {
        Printf(Tee::PriError, "HC is not in Halt state, can't be reset \n");
        return RC::HW_STATUS_ERROR;
    }
    return OK;
}

RC XhciController::StartStop(bool IsStart)
{
    RC rc;
    UINT32 reg;
    CHECK_RC( RegRd(XHCI_REG_OPR_USBCMD, &reg) );

    if ( IsStart )
    {   // Start
        if ( FLD_TEST_RF_NUM(XHCI_REG_OPR_USBCMD_RS, 0x1, reg) )
        {
            Printf(Tee::PriLow, "HC already started\n");
            return OK;
        }
        rc = WaitRegMem(m_CapLength+(XHCI_REG_OPR_USBSTS^XHCI_REG_TYPE_OPERATIONAL), RF_SHIFTMASK(XHCI_REG_OPR_USBSTS_HCH), 0, GetTimeout());
        if ( rc != OK )
        {
            Printf(Tee::PriError, "Wait on HCH = 1 timeout, could not start \n");
            return rc;
        }
        reg = FLD_SET_RF_NUM(XHCI_REG_OPR_USBCMD_RS, 0x1, reg);
        RegWr(XHCI_REG_OPR_USBCMD, reg);
    }
    else
    {   // Stop
        if ( FLD_TEST_RF_NUM(XHCI_REG_OPR_USBCMD_RS, 0x0, reg) )
        {
            Printf(Tee::PriLow, "HC already stopped, now wait for HCH = 1\n");
        }
        else
        {
            reg = FLD_SET_RF_NUM(XHCI_REG_OPR_USBCMD_RS, 0x0, reg);
            RegWr(XHCI_REG_OPR_USBCMD, reg);
        }
#ifdef SIM_BUILD
        Printf(Tee::PriNormal, "Stop HC, start to wait for HCH bit set... ");
        UINT32 usbSts;
        RegRd(XHCI_REG_OPR_USBSTS, & usbSts);
        while(FLD_TEST_RF_NUM(XHCI_REG_OPR_USBSTS_HCH, 0x0, usbSts))
        {   // infinite looop until HCH is set by HW
            RegRd(XHCI_REG_OPR_USBSTS, & usbSts);
        }
        Printf(Tee::PriNormal, "Exit \n");
#else
        rc = WaitRegMem(m_CapLength+(XHCI_REG_OPR_USBSTS^XHCI_REG_TYPE_OPERATIONAL),  RF_SHIFTMASK(XHCI_REG_OPR_USBSTS_HCH), 0, GetTimeout());
        if ( rc != OK )
        {
            Printf(Tee::PriError, "Wait on HCH = 1 timeout, Stop failed \n");
            return rc;
        }
#endif
    }
    return OK;
}

RC XhciController::GetExtHidDev(UINT08 SlotId, UsbHidDev** ppHid)
{
    RC rc;
    CHECK_RC(CheckDevValid(SlotId));
    if ( m_pUsbDevExt[SlotId]->GetDevClass() != UsbClassHid )
    {
        Printf(Tee::PriError, "Command only supported on Hid mouse \n");
        return RC::UNSUPPORTED_FUNCTION_BY_EXT_DEV;
    }
    *ppHid = (UsbHidDev*) m_pUsbDevExt[SlotId];
    return OK;
}

RC XhciController::ReadMouse(UINT08 SlotId, UINT32 RunSeconds)
{
    RC rc;
    UsbHidDev *pHidDev;
    CHECK_RC(GetExtHidDev(SlotId, &pHidDev));
    CHECK_RC(pHidDev->ReadMouse(RunSeconds));
    return OK;
}

RC XhciController::GetExtVideoDev(UINT08 SlotId, UsbVideoDev** ppVideo)
{
    RC rc;
    CHECK_RC(CheckDevValid(SlotId));
    if ( m_pUsbDevExt[SlotId]->GetDevClass() != UsbClassVideo )
    {
        Printf(Tee::PriError, "Command only supported on video device \n");
        return RC::UNSUPPORTED_FUNCTION_BY_EXT_DEV;
    }
    *ppVideo = (UsbVideoDev*) m_pUsbDevExt[SlotId];
    return OK;
}

RC XhciController::ReadWebCam(UINT08 SlotId, UINT32 MaxSecs, UINT32 MaxRunTds, UINT32 MaxAddTds)
{
    RC rc;
    UsbVideoDev *pVideoDev;
    CHECK_RC(GetExtVideoDev(SlotId, &pVideoDev));
    CHECK_RC(pVideoDev->ReadWebCam(MaxSecs, MaxRunTds, MaxAddTds));
    return OK;
}

RC XhciController::SaveFrame(UINT08 SlotId, const string fileName)
{
    RC rc;
    UsbVideoDev *pVideoDev;
    CHECK_RC(GetExtVideoDev(SlotId, &pVideoDev));
    CHECK_RC(pVideoDev->SaveFrame(fileName));
    return OK;
}

RC XhciController::GetExtHubDev(UINT08 SlotId, UsbHubDev** ppHub)
{
    RC rc;
    CHECK_RC(CheckDevValid(SlotId));
    if (m_pUsbDevExt[SlotId]->GetDevClass()!= UsbClassHub)
    {
        Printf(Tee::PriError, "Command only supported on HUB device \n");
        return RC::UNSUPPORTED_FUNCTION_BY_EXT_DEV;
    }
    *ppHub = (UsbHubDev*) m_pUsbDevExt[SlotId];
    return OK;
}

RC XhciController::ReadHub(UINT08 SlotId, UINT32 MaxSeconds)
{
    RC rc;
    UsbHubDev *pHub;
    CHECK_RC(GetExtHubDev(SlotId, &pHub));
    CHECK_RC(pHub->ReadHub(MaxSeconds));
    return OK;
}

RC XhciController::HubSearch(UINT08 SlotId)
{
    RC rc;
    UsbHubDev *pHub;
    CHECK_RC(GetExtHubDev(SlotId, &pHub));
    CHECK_RC(pHub->Search());
    return OK;
}

RC XhciController::HubReset(UINT08 SlotId, UINT08 Port)
{
    RC rc;
    UsbHubDev *pHub;
    CHECK_RC(GetExtHubDev(SlotId, &pHub));
    UINT08 dummySpeed;
    CHECK_RC(pHub->ResetHubPort(Port, &dummySpeed));
    return OK;
}

// ----------------------------------------------------------------------------
// For Configurable USB Device
// Cat.1 Register Read / Write
// Cat.2 Bulk EP in / out
RC XhciController::GetExtConfigurableDev(UINT08 SlotId, UsbConfigurable** ppDev)
{
    RC rc;
    CHECK_RC(CheckDevValid(SlotId));
    if (m_pUsbDevExt[SlotId]->GetDevClass()!= UsbConfigurableDev)
    {
        Printf(Tee::PriError, "Command only supported on USB Configurable device \n");
        return RC::UNSUPPORTED_FUNCTION_BY_EXT_DEV;
    }
    *ppDev = (UsbConfigurable*) m_pUsbDevExt[SlotId];
    return OK;
}

RC XhciController::DevRegRd(UINT08 SlotId, UINT32 Offset, UINT64 * pData)
{
    RC rc;
    UsbConfigurable *pDev;
    CHECK_RC(GetExtConfigurableDev(SlotId, &pDev));
    CHECK_RC(pDev->RegRd(Offset, pData));
    return OK;
}

RC XhciController::DevRegWr(UINT08 SlotId, UINT32 Offset, UINT64 Data)
{
    RC rc;
    UsbConfigurable *pDev;
    CHECK_RC(GetExtConfigurableDev(SlotId, &pDev));
    CHECK_RC(pDev->RegWr(Offset, Data));
    return OK;
}

RC XhciController::DataIn(UINT08 SlotId,
                      UINT08 Endpoint,
                      MemoryFragment::FRAGLIST * pFragList,
                      bool IsIsoch)
{
    RC rc;
    UsbConfigurable *pDev;
    CHECK_RC(GetExtConfigurableDev(SlotId, &pDev));
    CHECK_RC(pDev->DataIn(Endpoint, pFragList, IsIsoch));
    return OK;
}

RC XhciController::DataOut(UINT08 SlotId,
                       UINT08 Endpoint,
                       MemoryFragment::FRAGLIST * pFragList,
                       bool IsIsoch)
{
    RC rc;
    UsbConfigurable *pDev;
    CHECK_RC(GetExtConfigurableDev(SlotId, &pDev));
    CHECK_RC(pDev->DataOut(Endpoint, pFragList, IsIsoch));
    return OK;
}

RC XhciController::DataRetrieve(UINT08 SlotId,
                            UINT08 Endpoint,
                            bool IsDataOut,
                            MemoryFragment::FRAGLIST * pFragList,
                            UINT32 StreamId,
                            UINT32 NumTd)
{
    RC rc;
    TransferRing *pXferRing;
    CHECK_RC(FindXferRing(SlotId, Endpoint, IsDataOut, &pXferRing, StreamId));
    CHECK_RC(pXferRing->BufferRetrieve(pFragList, NumTd));
    return OK;
}

RC XhciController::DataRelease(UINT08 SlotId,
                           UINT08 Endpoint,
                           bool IsDataOut,
                           UINT32 StreamId,
                           UINT32 NumTd)
{
    RC rc;
    TransferRing *pXferRing;
    CHECK_RC(FindXferRing(SlotId, Endpoint, IsDataOut, &pXferRing, StreamId));
    CHECK_RC(pXferRing->BufferRelease(NumTd));
    return OK;
}

RC XhciController::InitDummyBuf(UINT32 BufferSize)
{
    RC rc;
    if (0 == BufferSize || m_DummyBufferSize )
    {   // free the memory on first time running or modification
        MemoryTracker::FreeBuffer(m_pDummyBuffer);
        m_pDummyBuffer = NULL;
        m_DummyBufferSize = 0;
    }
    if (BufferSize)
    {
        UINT32 addrBit = 32;
        CHECK_RC( MemoryTracker::AllocBufferAligned(BufferSize, &m_pDummyBuffer, 4096, addrBit, Memory::WB) );
        Memory::Fill08(m_pDummyBuffer, 0, BufferSize);
        m_DummyBufferSize = BufferSize;
    }
    if (m_pDummyBuffer)
    {
        Printf(Tee::PriNormal, "Dummy Buffer of %d Byte created @ 0x%llx \n",
               m_DummyBufferSize, Platform::VirtualToPhysical(m_pDummyBuffer));
    }
    return OK;
}

RC XhciController::GetDummyBuf(void ** ppBuf, UINT32 BufferSize, UINT32 Alignment)
{
    if (!ppBuf)
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    *ppBuf = NULL;
    if (!m_pDummyBuffer)
    {
        Printf(Tee::PriLow, "No dummy buffer exists, return NULL\n");
        return OK;
    }

    if (BufferSize > m_DummyBufferSize || 0 == BufferSize)
    {
        Printf(Tee::PriError, "Invalid length %d requested, valid[1-%d] \n", BufferSize, m_DummyBufferSize);
        return RC::BAD_PARAMETER;
    }

    UINT32 offset = DefaultRandom::GetRandom(0, m_DummyBufferSize - BufferSize);
    offset = offset - (offset % Alignment);
    *ppBuf = (void *) ((UINT08*)m_pDummyBuffer + offset);
    return OK;
}

RC XhciController::DumpAll()
{
    RC rc;
    // init the m_pRegisterBase; m_CapLength; m_DoorBellOffset; m_RuntimeOffset
    InitStates initState = m_InitState;
    if (m_InitState < INIT_BAR)
    {
        CHECK_RC(InitBar());
        m_InitState = INIT_BAR; // to cheat the following code
    }

    m_IsSkipRegWrInInit = true;
    CHECK_RC(InitRegisters());
    m_IsSkipRegWrInInit = false;

    Printf(Tee::PriNormal, " -= Registers =- \n");
    PrintReg();

    // dump the data structures
    // Devce context Base Address Array
    UINT32 reg32;
    CHECK_RC(RegRd(XHCI_REG_OPR_CONFIG, &reg32));
    UINT32 numSlot = RF_VAL(XHCI_REG_OPR_CONFIG_MAXSLOTEN, reg32);
    Printf(Tee::PriNormal, " -= Device Context Base Adddress Array ");
    Printf(Tee::PriNormal, "%d Entry (non-zero entries only) =- \n", numSlot);

    vector<void*> vpDevContext;
    vector<UINT32> vSlotId;
    if (numSlot)
    {
        CHECK_RC(RegRd(XHCI_REG_OPR_DCBAAP_HI, &reg32));
        PHYSADDR pDcbaa = reg32;
        pDcbaa <<= 32;
        CHECK_RC(RegRd(XHCI_REG_OPR_DCBAAP_LO, &reg32));
        pDcbaa |= reg32;

        void * pVirtDcbaa;
        CHECK_RC(Platform::MapDeviceMemory(&pVirtDcbaa, pDcbaa,
                                           8 * numSlot,
                                           Memory::UC, Memory::Readable));

        for (UINT32 i = 0; i < numSlot; i++)
        {
            UINT64 data64 = MEM_RD64((size_t)pVirtDcbaa + 8 * i);
            if (data64)
            {
                Printf(Tee::PriNormal, "[%03d] 0x%llx \n", i, data64);
                if (i)
                {   // Entry 0 is for scratch buffer
                    void * pVirtDevContext;
                    // map the address of this Device Context
                    CHECK_RC(Platform::MapDeviceMemory(&pVirtDevContext, data64,
                                                       (m_Is64ByteContext?64:32) * 32,
                                                       Memory::UC, Memory::Readable));

                    vpDevContext.push_back(pVirtDevContext);
                    vSlotId.push_back(i);
                }
            }
        }
        Platform::UnMapDeviceMemory(pVirtDcbaa);
    }

    // Device context
    Printf(Tee::PriNormal, " -= Device Context %d Entry =- \n", (UINT32)vpDevContext.size());
    if (vpDevContext.size())
    {
        DeviceContext myContext(m_Is64ByteContext);
        myContext.Init(32); // enable all 32 endpoints

        vector<UINT32> vData32;
        for (UINT32 i = 0; i < vpDevContext.size(); i++)
        {
            Printf(Tee::PriNormal, "[%03d] ", vSlotId[i]);
            // copy the Device Context Data to vector
            UINT32 tmp = (m_Is64ByteContext?64:32) * 32;
            for (UINT32 j = 0; j < tmp; j+=4)
            {
                vData32.push_back( (UINT32) MEM_RD32((size_t)(vpDevContext[i]) + j) );
            }
            Platform::UnMapDeviceMemory(vpDevContext[i]);
            // port the vector to Context object
            myContext.FromVector(&vData32, true);
            myContext.PrintInfo();
            vData32.clear();
        }
    }

    // Event Ring
    Printf(Tee::PriNormal, " -= Primary Event Ring =- \n");
    CHECK_RC(RegRd(XHCI_REG_ERSTSZ(0), &reg32));
    UINT32 segTblSize = RF_VAL(XHCI_REG_ERSTSZ_BITS, reg32);

    CHECK_RC(RegRd(XHCI_REG_ERSTBA1(0), &reg32));
    PHYSADDR pRing = reg32;
    pRing <<= 32;
    CHECK_RC(RegRd(XHCI_REG_ERSTBA(0), &reg32));
    pRing |= reg32;

    void * pVirtRing;
    CHECK_RC(Platform::MapDeviceMemory(&pVirtRing, pRing,
                                       XHCI_RING_ERST_ENTRY_SIZE * segTblSize,
                                       Memory::UC, Memory::Readable));
    void * pPreserve = pVirtRing;
    for (UINT32 i = 0; i < segTblSize; i++)
    {   // read the Physical address of Event Ring Segment
        PHYSADDR pSegment = MEM_RD32((size_t)pVirtRing + 4);
        pSegment <<= 32;
        pSegment |= MEM_RD32((size_t)pVirtRing);

        UINT32 segmentSize = MEM_RD32((size_t)pVirtRing + 8);
        CHECK_RC(DumpRing(pSegment, segmentSize, true));

        pVirtRing =(void*)( (size_t)pVirtRing + XHCI_RING_ERST_ENTRY_SIZE );
    }
    Platform::UnMapDeviceMemory(pPreserve);

    // unmap the memory mapped in InitBar() to exit gracefully
    m_InitState = initState;
    if (m_InitState < INIT_BAR)
    {
        UninitBar();
    }

    return OK;
}

RC XhciController::DumpRing(PHYSADDR pHead, UINT32 NumTrb, bool IsEventRing)
{
    RC rc;
    if (IsEventRing && !NumTrb)
    {   // event ring has no link trb at the tail, so we don't know where it ends
        Printf(Tee::PriError, "Number of TRB is missing for Event Ring dumping \n");
        return RC::BAD_PARAMETER;
    }
    Printf(Tee::PriNormal,
           "Dumping %sRing @ 0x%llx for %d entries\n",
           IsEventRing?"Event Ring ":"", pHead, NumTrb);
    // preserve the start address, use it to find out the completion of the ring enumeration
    PHYSADDR pRing = pHead;
    UINT32 count = 0;

    if (NumTrb == 0)
    {   // if not specify the segment size, we dump max 256 TRB
        NumTrb = 256;
    }
    void * pVirtRing = NULL;
    while ((count < NumTrb))
    {
        // map one TRB each time
        CHECK_RC(Platform::MapDeviceMemory(&pVirtRing, pHead,
                                           XHCI_TRB_LEN_BYTE,
                                           Memory::UC, Memory::Readable));
        UINT08 trbType = 0;
        rc = TrbHelper::GetTrbType((XhciTrb*) pVirtRing, &trbType);      // prevserve the type

        if (OK == rc)
        {
            Printf(Tee::PriNormal, "[%03d] ", count);
            TrbHelper::DumpTrbInfo((XhciTrb*)pVirtRing, true, Tee::PriNormal, true, false);

            if (trbType == XhciTrbTypeLink)
            {   // handle the jump
                TrbHelper::GetLinkTarget((XhciTrb*) pVirtRing, &pHead);
            }
            else
            {   // regular TRBs
                pHead += XHCI_TRB_LEN_BYTE;
            }
        }
        else
        {   // unknown TRBs / empty slots
            Printf(Tee::PriNormal, "\n");
            pHead += XHCI_TRB_LEN_BYTE;
        }
        Platform::UnMapDeviceMemory(pVirtRing);

        //if (( OK != rc )
        //    || ( !IsEventRing && (trbType > XhciTrbTypeCmdNoOp) ))
        //{
        //    Printf(Tee::PriError, "This doesn't look like a valid Command / Transfer Ring \n");
        //    return RC::BAD_PARAMETER;
        //}

        if (pRing == pHead)
        {   // Ring end
            break;
        }
        count++;
    }
    if (!IsEventRing)
    {
        Printf(Tee::PriNormal, "DumpRing finished\n ");
    }
    return OK;
}

// sample JS code to use this function
/*
var slotId = Xhci.GetNewSlot();
var endpoint = xxxx;
var isDataOut = xxxx;
for (var i = 0; i < 5; i ++)
{
    Xhci.InsertTd(slotId, endpoint, isDataOut, [1024*48], [0x8], true)
}
Xhci.Doorbell(slotId, endpoint, isDataOut);
Xhci.SendIsoCmd(slotId, 0, [endpoint*2+(isDataOut?0:1), true, 0, 500])
for (var i = 0; i < 500; i ++)
{
    var trb = new Array();
    if (Xhci.CheckXferEvent(0, slotId, endpoint isDataOut, trb))
    {
        // release the data buffer
        Xhci.DataDone(slotId, endpoint, isDataOut, *0, [], 1)
        // insert a new TD
        Xhci.InsertTd(slotId, endpoint, isDataOut, [1024*48], [0x8], true)
    }
}
*/
RC XhciController::SendIsoCmd(UINT08 SlotId, UINT32 Cmd, vector<UINT32> * pCmdData)
{
    LOG_ENT();
    RC rc = OK;
    UINT08 reqType, req;
    UINT16 value, index, length;

    // build up the command
    // see also RC XusbDevice::EngineIsoch()
    switch( Cmd )
    {
    case 0: //start
        if ( pCmdData->size() != 4 )
        {
            Printf(Tee::PriError, "Invalid parameter list \n");
            return RC::BAD_PARAMETER;
        }
        Printf(Tee::PriNormal, "Start ");
        reqType = 0x40;
        req = UsbIsochStart;
        value  = (*pCmdData)[1] & 0xff;     // SIA
        value <<= 8;
        value |= (*pCmdData)[0] & 0xff;     // epdci
        index = (*pCmdData)[2] & 0xffff;    // frameId
        length = (*pCmdData)[3] & 0xffff;   // numTd
        break;
    case 1: // stop
        if ( pCmdData->size() != 3 )
        {
            Printf(Tee::PriError, "Invalid parameter list \n");
            return RC::BAD_PARAMETER;
        }
        Printf(Tee::PriNormal, "Stop ");
        reqType = 0x40;
        req = UsbIsochStop;
        value  = (*pCmdData)[1] & 0xff;     // SIA
        value <<= 8;
        value |= (*pCmdData)[0] & 0xff;     // epdci
        index = (*pCmdData)[2] & 0xffff;    // frameId
        length = 0;                         // numTd
        break;
    case 2: // query_statistic
        if ( pCmdData->size() != 1 )
        {
            Printf(Tee::PriError, "Invalid parameter list \n");
            return RC::BAD_PARAMETER;
        }
        Printf(Tee::PriNormal, "Query ");
        reqType = 0xc0;
        req = UsbIsochQuery;
        value = (*pCmdData)[0] & 0xff;      // epdci
        index = 0;                          // frameId
        length = 0;                         // numTd
        break;
    case 3: // reset_statistic
        if ( pCmdData->size() != 1 )
        {
            Printf(Tee::PriError, "Invalid parameter list \n");
            return RC::BAD_PARAMETER;
        }
        Printf(Tee::PriNormal, "Reset ");
        reqType = 0x40;
        req = UsbIsochReset;
        value = (*pCmdData)[0] & 0xff;      // epdci
        index = 0;                          // frameId
        length = 0;                         // numTd
        break;
    default:
        Printf(Tee::PriError, "Unsupport command %d \n", Cmd);
        return RC::BAD_PARAMETER;
    }

    Printf(Tee::PriNormal,
           "ReqType 0x%02x, Req %d, Value 0x%04x, Index 0x%04x, Length 0x%04x \n",
           reqType, req, value, index, length);
    // send the command
    TransferRing * pXferRing;
    CHECK_RC(FindXferRing(SlotId, 0, true, & pXferRing));

    CHECK_RC(pXferRing->InsertSetupTrb(reqType, req, value, index, length));
    void * pData = NULL;
    if ( reqType & 0x80 )
    {
        CHECK_RC( MemoryTracker::AllocBuffer(64, &pData, true, 32, Memory::UC) );
        Memory::Fill08(pData, 0, 64);
        CHECK_RC_CLEANUP(pXferRing->InsertDataStateTrb(pData, 64, false));
    }
    CHECK_RC_CLEANUP(pXferRing->InsertStastusTrb(false));

    CHECK_RC_CLEANUP(RingDoorBell(SlotId, 0, true));
    CHECK_RC_CLEANUP(WaitXfer(SlotId, 0, true));

Cleanup:
    if ( pData )
    {
        MemoryTracker::FreeBuffer(pData);
    }

    LOG_ENT();
    return rc;
}

RC XhciController::Reset()
{   // todo: reset HC and all the data structures, go back to Init() state
    ResetHc();
    return OK;
}

RC XhciController::GetIsr(ISR* Func) const
{
    LOG_ENT();
    *Func = &XhciControllerMgr::IsrCommon0;
    Printf(Tee::PriLow, "ISR @%p is used for XHCI IRQ mode", *Func);
    LOG_EXT();
    return OK;
}

RC XhciController::MailboxTx(UINT32 Data, bool IsSetId /*=false*/)
{
    LOG_ENT();
    RC rc;
    if (!m_pBar2 && !m_OffsetPciMailbox)
    {   // Mailbox is not enabled, do nothing
        return OK;
    }

    UINT32 data32;
    UINT32 msElapse = 0;

    if ( IsSetId )
    {   // check owner = none
        CHECK_RC(MailboxRegRd32(m_OffsetPciMailbox + USB3_PCI_OFFSET_OWNER, &data32));
        while ( data32 != USB3_MAILBOX_OWNER_NONE )
        {
            if ( msElapse >= GetTimeout() )
            {
                Printf(Tee::PriError,
                       "Mailbox: Timeout on waiting for Owner NONE, Owner: %d \n",
                       data32);
                return RC::HW_STATUS_ERROR;
            }
            Tasker::Sleep(1);
            msElapse ++;
            CHECK_RC(MailboxRegRd32(m_OffsetPciMailbox + USB3_PCI_OFFSET_OWNER, &data32));
        }
        msElapse = 0;
        // write ID semaphore
        CHECK_RC(MailboxRegWr32(m_OffsetPciMailbox + USB3_PCI_OFFSET_OWNER, USB3_MAILBOX_OWNER_ID));
        // check if the value updated
        CHECK_RC(MailboxRegRd32(m_OffsetPciMailbox + USB3_PCI_OFFSET_OWNER, &data32));
        while ( data32 != USB3_MAILBOX_OWNER_ID )
        {
            if ( msElapse >= GetTimeout() )
            {
                Printf(Tee::PriError, "Time out on waiting for Owner update \n");
                return RC::HW_STATUS_ERROR;
            }
            Tasker::Sleep(1);
            msElapse ++;
            CHECK_RC(MailboxRegRd32(m_OffsetPciMailbox + USB3_PCI_OFFSET_OWNER, &data32));
        }
    }

    // write Data to Data In register
    CHECK_RC(MailboxRegWr32(m_OffsetPciMailbox + USB3_PCI_OFFSET_DATA_IN, Data));
    // clear Data_OUT register for FW
    CHECK_RC(MailboxRegWr32(m_OffsetPciMailbox + USB3_PCI_OFFSET_DATA_OUT, 0));

    // set interrupt bit for notification
    // set the Msg Box Interrupt bit and Target to Falcon
    CHECK_RC(MailboxRegWr32(m_OffsetPciMailbox + USB3_PCI_OFFSET_MAILBOX_CMD, (1<<31)|(1<<27)));

    LOG_EXT();
    return OK;
}

RC XhciController::MailboxAckCheck()
{
    LOG_ENT();

    RC rc;
    if (!m_pBar2 && !m_OffsetPciMailbox)
    {   // Mailbox is not enabled, do nothing
        return OK;
    }

    UINT32 data32;
    UINT32 msElapse = 0;
    // wait for ACK from Data_Out
    CHECK_RC(MailboxRegRd32(m_OffsetPciMailbox + USB3_PCI_OFFSET_DATA_OUT, &data32));
    while ( 0 == ( data32 >> 24 ) )
    {
        if ( msElapse >= GetTimeout() )
        {
            Printf(Tee::PriError, "Time out on waiting for DATA_OUT to be set \n");
            return RC::HW_STATUS_ERROR;
        }
        Tasker::Sleep(1);
        msElapse ++;
        CHECK_RC(MailboxRegRd32(m_OffsetPciMailbox + USB3_PCI_OFFSET_DATA_OUT, &data32));
    }

    // clear the interrupt bits and ID register, return data
    CHECK_RC(MailboxRegWr32(m_OffsetPciMailbox + USB3_PCI_OFFSET_MAILBOX_CMD, 1<<31));
    CHECK_RC(MailboxRegWr32(m_OffsetPciMailbox + USB3_PCI_OFFSET_OWNER, 0));

    if ( SW_FW_MBOX_CMD_ACK != (data32 >> 24) )
    {
        Printf(Tee::PriError, "FW ACK error 0x%x \n", data32);
        return RC::HW_STATUS_ERROR;
    }

    LOG_EXT();
    return OK;
}

RC XhciController::FindMailBoxCap()
{
    RC rc;
    if ( m_pBar2 || m_OffsetPciMailbox )
    {
        Printf(Tee::PriDebug, "Cap already found, skipping... \n");
        return OK;
    }

    UINT32 capPtr;
    UINT32 val32;
    CHECK_RC(CfgRd32(USB3_PCI_CAP_PTR_OFFSET, &capPtr));
    capPtr &= 0xfc;
    UINT32 searchCnt = 0;       // to prevent a inifinate loop

    while(capPtr != 0)
    {
        CHECK_RC( CfgRd32(capPtr, &val32) );
        Printf(Tee::PriLow,
               "Cap found at 0x%02x, ID = 0x%02x, Len = 0x%02x \n",
               capPtr, val32&0xff, (val32>>16)&0xff );

        if ( (val32 & 0xff) == 0x9
             && ( (val32>>16)& 0xff ) == 0x14 )
        {   // we found the vendor specific CAP
            m_OffsetPciMailbox = capPtr;
            break;
        }
        searchCnt++;
        if ( searchCnt > 10 )
        {
            Printf(Tee::PriError, "Max search count reached, no Cap found \n");
            return RC::HW_STATUS_ERROR;
        }
        capPtr = (val32 >> 8) & 0xff;
    }

    if ( !m_OffsetPciMailbox )
    {
        Printf(Tee::PriError, "Message Box Vendor Specific Capability not found \n");
        return RC::HW_STATUS_ERROR;
    }

    return OK;
}

RC XhciController::SetCErr(UINT08 Cerr)
{
    m_CErr = Cerr;
    Printf(Tee::PriNormal, "  CErr set to %d \n", m_CErr);
    return OK;
}

UINT32 XhciController::CsbGetBufferSize()
{   // ideally we could set it to 65536, however on T114 it works with Max value of 0xfff0
    return 65536-16;
}

RC XhciController::CsbDmaPartition()
{   // create partition in mempool. total  size = 64KB,
    // we have three type of partition: IDirect, L2iMEM and DDirect
    // for instructions, data cache and bulk data respectively
    // for register definition, see also //sw/dev/mcp_drv/usb3/xhcifw/include/lwcsb.h
    LOG_ENT();
    RC rc;
    UINT32 val;
    CHECK_RC(CsbRd(LW_PROJ__XUSB_CSB_MEMPOOL_RAM, &val));
    val = FLD_SET_RF_NUM(LW_PROJ__XUSB_CSB_MEMPOOL_RAM_SIZE, 2, val);
    CHECK_RC(CsbWr(LW_PROJ__XUSB_CSB_MEMPOOL_RAM, val));

    val = RF_NUM(LW_PROJ__XUSB_CSB_MEMPOOL_DDIRECT_BASE, 0);
    val = FLD_SET_RF_NUM(LW_PROJ__XUSB_CSB_MEMPOOL_DDIRECT_SIZE, CsbGetBufferSize()>>4, val);
    CHECK_RC(CsbWr(LW_PROJ__XUSB_CSB_MEMPOOL_DDIRECT, val));
    //       CsbWr(LW_PROJ__XUSB_CSB_MEMPOOL_DDIRECT, 0xffff0000);

    // kind of init
    m_PendingDmaCmd = 0;
    m_DmaCount = 0;
    bool isDumy;
    CsbDmaCheckStatus(&isDumy);
    LOG_EXT();
    return OK;
}

RC XhciController::CsbDmaFillPattern(vector<UINT32> * pPattern32)
{   // fill the DDirect partition with data pattern by PIO auto increamental address mode
    LOG_ENT();
    RC rc;
    UINT32 val;
    //set start offset and enable auto-incr for write address
    CHECK_RC(CsbRd(LW_PROJ__XUSB_CSB_MEMPOOL_REGACCESS_MEMC, &val));
    val = FLD_SET_RF_NUM(LW_PROJ__XUSB_CSB_MEMPOOL_REGACCESS_MEMC_ADDR, 0, val);
    val = FLD_SET_RF_NUM(LW_PROJ__XUSB_CSB_MEMPOOL_REGACCESS_MEMC_AINCW, 1, val);
    CHECK_RC(CsbWr(LW_PROJ__XUSB_CSB_MEMPOOL_REGACCESS_MEMC, val));

    UINT32 patternSize = pPattern32->size();
    for( UINT32 i = 0; i < CsbGetBufferSize()/4; i++ )
    {
        val = (*pPattern32)[i%patternSize];
        CHECK_RC(CsbWr(LW_PROJ__XUSB_CSB_MEMPOOL_REGACCESS_MEMD, val));
    }
    LOG_EXT();
    return OK;
}

RC XhciController::CsbDmaReadback(vector<UINT32> * pData32)
{   // read the DDiret partition by PIO auto increamental address mode
    LOG_ENT();
    RC rc;
    UINT32 val;
    pData32->clear();
    //set start address and enable auto-incr for read address
    CHECK_RC(CsbRd(LW_PROJ__XUSB_CSB_MEMPOOL_REGACCESS_MEMC, &val));
    val = FLD_SET_RF_NUM(LW_PROJ__XUSB_CSB_MEMPOOL_REGACCESS_MEMC_ADDR, 0, val);
    val = FLD_SET_RF_NUM(LW_PROJ__XUSB_CSB_MEMPOOL_REGACCESS_MEMC_AINCR, 1, val);
    CHECK_RC(CsbWr(LW_PROJ__XUSB_CSB_MEMPOOL_REGACCESS_MEMC, val));

    for( UINT32 i = 0; i < CsbGetBufferSize()/4; i++ )
    {
        CHECK_RC(CsbRd(LW_PROJ__XUSB_CSB_MEMPOOL_REGACCESS_MEMD, &val));
        pData32->push_back(val);
    }
    LOG_EXT();
    return OK;
}

RC XhciController::CsbDmaTrigger(PHYSADDR pTargerAddr, bool IsWrite)
{   // trigger a DMA from MEMpool to system MEM, check if command FIFO is full (free count)
    LOG_ENT();
    // check free command slot count
    RC rc;
    UINT32 val;
    UINT32 freeCount;
    CHECK_RC(CsbRd(LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_STATUS, &freeCount));
    if ( FLD_TEST_RF_NUM(LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_STATUS_FREE_COUNT, 0, freeCount) )
    {   // no space in command fifo, exit
        Printf(Tee::PriWarn, "  CMD FIFO full \n");
        return OK;
    }

    // program the transfer size
    val = RF_NUM(LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_ATTR_SIZE, CsbGetBufferSize()>>4);
    CsbWr(LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_ATTR, val);
    Printf(Tee::PriLow, "  0x%08x -> 0x%x \n", val, LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_ATTR);

    // program the physical address
    UINT32 tmp = pTargerAddr;
    CsbWr(LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_BASE_LO, tmp);
    Printf(Tee::PriLow, "  0x%08x -> 0x%x \n", tmp, LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_BASE_LO);

    val = RF_NUM(LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_BASE_HI_ADDR, 0);
    CsbWr(LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_BASE_HI, val);
    Printf(Tee::PriLow, "  0x%08x -> 0x%x \n", val, LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_BASE_HI);

    // trigger the DMA in a loop till the FREE_COUNT for command FIFO reahces 0
    // Set offset of MEM pool
    val = RF_NUM(LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_TRIG_LOCAL_OFFSET, 0);
    if ( IsWrite )
    {
        //bit 0  :  0=DDIRECT , 1=IDIRECT
        //bit 1  :  0=WRITE   , 1=READ
        //bit 2  :  0=NO_RESULT   , 1=RESULT
        //bit 3  :  0=normal addr   , 1=use aperture translation
        //bit 7-4  : Rsvd=0, except for fence
        val = FLD_SET_RF_NUM(LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_TRIG_ACTION, 0x4, val);
    }
    else
    {   // read
        val = FLD_SET_RF_NUM(LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_TRIG_ACTION, 0x6, val);
    }
    do{
        CsbWr(LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_TRIG, val);
        Printf(Tee::PriLow, "  0x%08x -> 0x%x \n", val, LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_TRIG);

        m_PendingDmaCmd++;
        m_DmaCount++;
        Printf(Tee::PriLow, "  pending cmd %d. total DMA %lld, freeCount %d . %s command inserted, \n",
               m_PendingDmaCmd, m_DmaCount, freeCount, IsWrite?"Write":"Read");
        // update the free count
        CsbRd(LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_STATUS, &freeCount);
        freeCount = RF_VAL(LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_STATUS_FREE_COUNT, freeCount);
    }while( freeCount );

    LOG_EXT();
    return OK;
}

RC XhciController::CsbDmaCheckStatus(bool *pIsComplete, UINT64 *pDmaCount, UINT32 *pFreeCount)
{
    RC rc = OK;
    UINT32 val;
    if ( pDmaCount )
    {
        *pDmaCount = m_DmaCount;
    }
    if ( !m_PendingDmaCmd )
    {
        *pIsComplete = true;
        return OK;
    }

    bool isComplete = false;
    *pIsComplete = false;
    // besides the Command FIFO, MEM pool has a result FIFO which has 8 slots
    for ( ; m_PendingDmaCmd > 0; m_PendingDmaCmd-- )
    {
        CHECK_RC(CsbRd(LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_RESULT, &val));
        if  ( FLD_TEST_RF_NUM(LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_RESULT_ERR, 1, val) )
        {
            Printf(Tee::PriError, "Error in DMA %d, status: 0x%08x \n", m_PendingDmaCmd, val);
            if ( OK == rc )
            {
                rc = RC::HW_STATUS_ERROR;
            }
        }
        isComplete = FLD_TEST_RF_NUM(LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_RESULT_VLD, 1, val);
        if ( isComplete )
        {   // clear status by popping it
            val = RF_NUM(LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_RESULT_POP, 1);
            CsbWr(LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_RESULT, val);
            // if ( 1 == m_PendingDmaCmd )
            {   // all status has shown up in status FIFO, we can say it completed
                *pIsComplete = isComplete;
            }
        }
        else
        {
            Printf(Tee::PriNormal, "  Pending Cmd %d! \n",  m_PendingDmaCmd);
            break;
        }
    }
    UINT32 freeCount;
    CsbRd(LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_STATUS, &freeCount);
    freeCount = RF_VAL(LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_STATUS_FREE_COUNT, freeCount);
    if ( pFreeCount )
    {
        *pFreeCount = freeCount;
    }

    Printf(Tee::PriLow, "  FreeCount %d, Pending Cmd %d \n", freeCount, m_PendingDmaCmd);
    return rc;
}

RC XhciController::FindTrb(PHYSADDR pPhyTrb)
{   // find given TRB in transfer rings, print ring info if found
    RC rc = OK;
    bool isFound = false;
    void * pVa = MemoryTracker::PhysicalToVirtual(pPhyTrb);
    UINT32 dummySeg, dummyIndex;
    UINT32 i = 0;
    for (; i <m_vpRings.size(); i++)
    {
        if (OK == m_vpRings[i]->FindTrb((XhciTrb*)pVa, &dummySeg, &dummyIndex, true))
        {
            isFound = true;
            break;
        }
    }
    if ( isFound )
    {
        Printf(Tee::PriNormal, "Found TRB 0x%0x @ %d:%d of ", (UINT32)pPhyTrb, dummySeg, dummyIndex);
        m_vpRings[i]->PrintTitle(Tee::PriNormal);
        TrbHelper::DumpTrbInfo((XhciTrb*)pVa,  true, Tee::PriNormal,  true,  false);
    }
    else
    {
        Printf(Tee::PriNormal, "ERROR: TRB 0x%0x not found! \n", (UINT32)pPhyTrb);
    }
    return rc;
}

RC XhciController::TrTd(UINT32 SlotId, UINT32 Ep, bool IsDataOut, UINT32 Num)
{
    RC rc;
    TransferRing * pXferRing;
    CHECK_RC( FindXferRing(SlotId, Ep, IsDataOut, & pXferRing) );
    CHECK_RC( pXferRing->TruncateTD(Num) );
    return OK;
}

// send SCSI write 10 command but not wait for completion
RC XhciController::BulkSendCbwWrite10(UINT32 SlotId, UINT32 Lba, UINT32 NumSectors, EP_INFO * pEpInfo)
{
    RC rc = OK;
    UsbStorage *pStor;
    CHECK_RC(GetExtStorageDev(SlotId, &pStor));
    vector<UINT08> vCbw;
    CHECK_RC(pStor->CmdWrite10(Lba, NumSectors, NULL, &vCbw));
    CHECK_RC(pStor->XferCbw(&vCbw, NULL, pEpInfo));
    return rc;
}

// send SCSI read 10 command but not wait for completion
RC XhciController::BulkSendCbwRead10(UINT32 SlotId, UINT32 Lba, UINT32 NumSectors, EP_INFO * pEpInfo)
{
    RC rc = OK;
    UsbStorage *pStor;
    CHECK_RC(GetExtStorageDev(SlotId, &pStor));
    vector<UINT08> vCbw;
    CHECK_RC(pStor->CmdRead10(Lba, NumSectors, NULL, &vCbw));
    CHECK_RC(pStor->XferCbw(&vCbw, NULL, pEpInfo));
    return rc;
}

RC XhciController::BulkGetInEpInfo(UINT08 SlotId,  EP_INFO * pEpInfo)
{
    RC rc = OK;
    UsbStorage *pStor;
    CHECK_RC(GetExtStorageDev(SlotId, &pStor));
    CHECK_RC(pStor->GetInEpInfo(pEpInfo));
    return rc;
}

RC XhciController::BulkSendCsw(UINT32 SlotId, EP_INFO * pEpInfo)
{
    RC rc = OK;
    UsbStorage *pStor;
    CHECK_RC(GetExtStorageDev(SlotId, &pStor));
    CHECK_RC(pStor->XferCsw(NULL, NULL, NULL, pEpInfo));
    return rc;
}

RC XhciController::SetConfiguration(UINT32 SlotId, UINT32 CfgIndex)
{
    RC rc = OK;
    UINT08 speed;
    UINT32 routeString;
    if ( NULL == m_pUsbDevExt[SlotId] )
    {
        Printf(Tee::PriError, "Slot ID %d invalid \n", SlotId);
        return RC::BAD_PARAMETER;
    }
    m_pUsbDevExt[SlotId]->GetProperties(&speed,  &routeString);

    // remove all the Transfer Rings associated with this SlotId.
    CHECK_RC(RemoveRingObj(SlotId,  XHCI_DEL_MODE_SLOT_EXPECT_EP0));

    // delete the old UsbDevExt object
    delete m_pUsbDevExt[SlotId];
    m_pUsbDevExt[SlotId] = NULL;

    UsbDevExt *pUsbDev;
    CHECK_RC( UsbDevExt::Create(this, SlotId, speed, routeString, &pUsbDev, CfgIndex) );
    // save the external USB device
    m_pUsbDevExt[SlotId] = pUsbDev;

    Printf(Tee::PriNormal, "Cfg changed to %d for Dev on SlotId %d \n", CfgIndex, SlotId);
    return rc;
}

// see also USB2.0 spec - Table 11-24. Test Mode Selector Codes
RC XhciController::EnablePortTestMode(UINT32 PortIndex, UINT08 TestMode, UINT32 RouteSting /* = 0x0 */)
{
    RC rc;
    UINT32 portIndex = PortIndex;
    if ( (TestMode < 1) || (TestMode > 8) )
    {
        Printf(Tee::PriNormal, "Invalid Test Mode %d \n", TestMode);
        Printf(Tee::PriNormal,
               "  1-J_STATE, 2-K_STATE, 3-SE0_NAK, 4-Packet, 5-FORCE_ENABLE\n"
               "  6-HS_SUSPEND_RESUME, 7-SINGLE_STEP_GET_DEV_DESC, 8-SINGLE_STEP_SET_FEATURE\n");
        return RC::BAD_PARAMETER;
    }

    if ( m_TestModePortIndex )
    {
        Printf(Tee::PriNormal,
               "Port %d is already in TestMode, call DisablePortTestMode() \n",
               m_TestModePortIndex);
        return RC::HW_STATUS_ERROR;
    }

    if ( TestMode > 5 )
    {   // private modes
        // see also http://git-master/r/#/c/360960/3/drivers/usb/class/usbhct.c
        UINT32 slotId;
        void * pDummy;
        switch ( TestMode )
        {
        case 6:
            Printf(Tee::PriNormal, "HS_HOST_PORT_SUSPEND_RESUME: Suspend the port\n");
            UINT32 portSc;
            //xusb_suspend_port(hcd, regs);
            CHECK_RC( RegRd(XHCI_REG_PORTSC(portIndex), &portSc) );
            portSc &= ~XHCI_REG_PORTSC_WORKING_MASK;
            portSc = FLD_SET_RF_NUM(XHCI_REG_PORTSC_PLS, 0x3,  portSc);
            portSc = FLD_SET_RF_NUM(XHCI_REG_PORTSC_PORT_LWS, 0x1,  portSc);
            CHECK_RC( RegWr(XHCI_REG_PORTSC(portIndex), portSc) );
            Printf(Tee::PriNormal, "Sleep for 15 sec\n");
            Tasker::Sleep(15000);

            Printf(Tee::PriNormal, "Resume the port\n");
            //xusb_force_resume_controller(hcd, regs);
            CHECK_RC( RegRd(XHCI_REG_PORTSC(portIndex), &portSc) );
            portSc &= ~XHCI_REG_PORTSC_WORKING_MASK;
            portSc = FLD_SET_RF_NUM(XHCI_REG_PORTSC_PLS, 0xf,  portSc);
            portSc = FLD_SET_RF_NUM(XHCI_REG_PORTSC_PORT_LWS, 0x1,  portSc);
            CHECK_RC( RegWr(XHCI_REG_PORTSC(portIndex), portSc) );
            Tasker::Sleep(20);
            CHECK_RC( RegRd(XHCI_REG_PORTSC(portIndex), &portSc) );
            portSc &= ~XHCI_REG_PORTSC_WORKING_MASK;
            portSc = FLD_SET_RF_NUM(XHCI_REG_PORTSC_PLS, 0x0,  portSc);
            portSc = FLD_SET_RF_NUM(XHCI_REG_PORTSC_PORT_LWS, 0x1,  portSc);
            CHECK_RC( RegWr(XHCI_REG_PORTSC(portIndex), portSc) );
            Printf(Tee::PriNormal, "Take test measurment in 15 secs \n");
            Tasker::Sleep(15000);
        break;
        case 7:
            Printf(Tee::PriNormal, "SINGLE_STEP_GET_DEV_DESC\n");
            Printf(Tee::PriNormal, "Waiting for 15 secs before start...\n");
            Tasker::Sleep(15000);
            Printf(Tee::PriNormal, "Sending device desc...\n");
            CHECK_RC(GetSlotIdByPort(portIndex, &slotId, RouteSting));
            CHECK_RC(MemoryTracker::AllocBufferAligned(8, &pDummy, 16, 32, Memory::UC));
            CHECK_RC(ReqGetDescriptor(slotId, UsbDescriptorDevice, 0, pDummy, 8));
            MemoryTracker::FreeBuffer(pDummy);
            Printf(Tee::PriNormal, "Take test measurment in 15 secs \n");
            Tasker::Sleep(15000);
            break;
        case 8:
            Printf(Tee::PriNormal, "SINGLE_STEP_SET_FEATURE\n");
            CHECK_RC(GetSlotIdByPort(portIndex, &slotId, RouteSting));
            CHECK_RC(MemoryTracker::AllocBufferAligned(8, &pDummy, 16, 32, Memory::UC));
            CHECK_RC(ReqGetDescriptor(slotId, UsbDescriptorDevice, 0, pDummy, 8));
            Printf(Tee::PriNormal, "Waiting for 15 secs before start...\n");
            Tasker::Sleep(15000);
            CHECK_RC(ReqGetDescriptor(slotId, UsbDescriptorDevice, 0, pDummy, 8));
            Printf(Tee::PriNormal, "Take test measurment in 15 secs \n");
            Tasker::Sleep(15000);
            MemoryTracker::FreeBuffer(pDummy);
            break;
        default:
            Printf(Tee::PriError, "Ilwliad test mode %d\n", TestMode);
            return RC::BAD_PARAMETER;
        }
    }
    else
    {
        UINT16 portVer;
        if ( portIndex != 0xff )
        {
            CHECK_RC(GetPortProtocol(portIndex, &portVer));
            if (portVer >= 0x300)
            {
                Printf(Tee::PriNormal, "TestMode can not be enabled on SS port %d \n", portIndex);
                return RC::UNSUPPORTED_FUNCTION;
            }
            m_TestModePortIndex = portIndex;
        }
        else
        {   // set up the bit mask for all HS ports
            portIndex = 1<<31;
            for ( UINT32 i = 1; i <= m_MaxPorts; i++ )
            {
                CHECK_RC(GetPortProtocol(i, &portVer));
                if (portVer < 0x300)
                {
                    portIndex |= 1<<i;
                }
            }
        }

        UINT32 portSc;
        // set pp = 0 for all ports
        for ( UINT32 i = 1; i <= m_MaxPorts; i++ )
        {
            CHECK_RC( RegRd(XHCI_REG_PORTSC(i), &portSc) );
            if ( FLD_TEST_RF_NUM(XHCI_REG_PORTSC_PP, 1, portSc ) )
            {
                portSc &= ~XHCI_REG_PORTSC_WORKING_MASK;
                portSc = FLD_SET_RF_NUM(XHCI_REG_PORTSC_PP, 0, portSc);
                CHECK_RC(RegWr(XHCI_REG_PORTSC(i), portSc));
            }
        }
        // Set the Run/Stop (R/S) bit in the USBCMD register to 0
        CHECK_RC(StartStop(false));

        if ( portIndex & (1<<31) )
        {   // use PortIndex as a bit mask for port index
            for ( UINT32 i = 1; i <= m_MaxPorts; i++ )
            {
                if ( portIndex & ( 1 << i ) )
                {
                    CHECK_RC( RegRd(XHCI_REG_PORTPMSC(i), &portSc) );
                    portSc = FLD_SET_RF_NUM(31:28, TestMode, portSc);
                    CHECK_RC(RegWr(XHCI_REG_PORTPMSC(i), portSc));
                    Printf(Tee::PriNormal, "TestMode %d on port %d \n", TestMode, i);
                    m_TestModePortIndex = i;    // dummy index just for disable
                }
            }
        }
        else
        {
            CHECK_RC( RegRd(XHCI_REG_PORTPMSC(portIndex), &portSc) );
            portSc = FLD_SET_RF_NUM(31:28, TestMode, portSc);
            CHECK_RC(RegWr(XHCI_REG_PORTPMSC(portIndex), portSc));
            Printf(Tee::PriNormal, "TestMode %d on port %d \n", TestMode, portIndex);
        }

        if ( 5== TestMode )
        {
        // if the selected test is Test_Force_Enable, then after selecting the test the
        //Run/Stop (R/S) bit in the USBCMD register shall then be transitioned back to 1 by sw
            CHECK_RC(StartStop(true));
        }
    }

    return OK;
}

RC XhciController::DisablePortTestMode()
{
    if ( !m_TestModePortIndex )
    {
        Printf(Tee::PriNormal,
               "No port is in TestMode, exit...  \n");
        return OK;
    }
    if (OK == ResetHc())
    {
        Printf(Tee::PriNormal, "HC reset, Test Mode disabled on port %d\n", m_TestModePortIndex);
        m_TestModePortIndex = 0;
    }
    return OK;
}

RC XhciController::EnableHubPortTestMode(UINT32 RootHubPort,
                                         UINT08 TestMode,
                                         UINT32 RouteString)
{
    RC rc;
    UINT32 routeString;
    UINT08 hubPort;
    // find the most upper port index
    CHECK_RC(ParseRouteSting(RouteString, &routeString, &hubPort));
    Printf(Tee::PriLow,
           "  Path to target HUB is 0x%x, enable Test mode %d on Port %d\n",
           routeString, TestMode, hubPort);

    // enumerate the host HUB of the target port
    CHECK_RC(UsbCfg(RootHubPort, true, routeString));
    UINT08 hubSlotId;
    // find the Target port's parent HUB's slotID
    CHECK_RC(FindHub(RootHubPort, routeString, &hubSlotId));

    UsbHubDev *pHub;
    CHECK_RC(GetExtHubDev(hubSlotId, &pHub));
    CHECK_RC(pHub->EnableTestMode(hubPort, TestMode));

    return OK;
}

RC XhciController::DisableHubPortTestMode(UINT32 RootHubPort, UINT32 RouteString)
{
    RC rc;
    UINT32 routeString;
    UINT08 hubPort;    //dummy
    // find the most upper port index
    CHECK_RC(ParseRouteSting(RouteString, &routeString, &hubPort));
    Printf(Tee::PriLow,
           "  Reset HUB on RootPort %d, HubPath 0x%x\n",
           RootHubPort, routeString);

    CHECK_RC(ResetPort(RootHubPort, routeString));
    return OK;
}

RC XhciController::GetSlotIdByPort(UINT32 Port, UINT32 *pSlotId, UINT32 RouteSting /*= 0x0*/)
{
    if ( !pSlotId )
    {
        Printf(Tee::PriError, "NULL pointer \n");
        return RC::BAD_PARAMETER;
    }
    if (!m_pDevContextArray)
    {
        Printf(Tee::PriError, "Controller / DeviceContext not init \n");
        return RC::WAS_NOT_INITIALIZED;
    }

    RC rc;
    *pSlotId = 0;
    vector<UINT08> vSlotIds;
    CHECK_RC(m_pDevContextArray->GetActiveSlots(&vSlotIds));

    DeviceContext * pDevContext;
    UINT08 port;
    UINT32 routeString;
    for ( UINT32 i = 0; i < vSlotIds.size(); i++ )
    {
        CHECK_RC(m_pDevContextArray->GetEntry(vSlotIds[i], &pDevContext));
        CHECK_RC(pDevContext->GetRootHubPort(&port));
        CHECK_RC(pDevContext->GetRouteString(&routeString));
        if ( ( port == Port ) && ( routeString == RouteSting ) )
        {   // find
            *pSlotId = vSlotIds[i];
            break;
        }
    }
    return OK;
}

RC XhciController::SetSSPortMap(UINT32 PortMap)
{
    m_SSPortMap = PortMap;
    return OK;
}

RC XhciController::SetBar2(UINT32 BAR2)
{
    m_PhysBAR2 = BAR2;
    return OK;
}

RC XhciController::IsDevConn(UINT32 Port, bool *pIsDevConn)
{
    RC rc = OK;
    UINT32 val;

    *pIsDevConn = false;
    CHECK_RC(RegRd(XHCI_REG_PORTSC(Port), &val));
    if ( (FLD_TEST_RF_NUM(XHCI_REG_PORTSC_CCS, 1, val))
          && (FLD_TEST_RF_NUM(XHCI_REG_PORTSC_PR, 0, val)) )
    {
        *pIsDevConn = true;
    }

    return rc;
}

RC XhciController::GetTimeStamp(TimeStamp* pData /*= NULL*/)
{
    RC rc = OK;

    UINT32 dataLen = XHCI_GET_EXTND_PROP_CONTEXT_SIZE_GET_TIME_STAMP;
    void * pDataBuf = NULL;
    CHECK_RC( MemoryTracker::AllocBufferAligned(dataLen,
                                                (void**)&pDataBuf,
                                                XHCI_ALIGNMENT_GET_EXTENED_PROPERTY,
                                                32,
                                                Memory::UC) );
    Memory::Fill08(pDataBuf, 0, dataLen);
    DEFER
    {
        MemoryTracker::FreeBuffer(pDataBuf);
    };
    CHECK_RC(CmdGetExtendedProperty( 0,
                                     0,
                                     EXTND_PROP_ECI_TIMESTAMP,
                                     EXTND_PROP_SUBTYPE_GETTIMESTAMP,
                                     (UINT08*)pDataBuf));

    UINT32 *pData32 = reinterpret_cast<UINT32*>(pDataBuf);
    TimeStamp times;
    times.systime = ((UINT64)pData32[0] << 32) | pData32[1];
    times.busInterCount = pData32[3] & 0x4FFF0000;
    times.delta = pData32[3] & 0x00001FFF;

    if (pData)
    {
        pData->systime = times.systime;
        pData->busInterCount = times.busInterCount;
        pData->delta = times.delta;
    }
    else
    {
        Printf(Tee::PriNormal, "Get time stamp resulte: \n");
        Printf(Tee::PriNormal, "\tsystime: %lld\n"
                               "\tbusInterCount: %d\n"
                               "\tdelta: %d\n",
                               times.systime,
                               times.busInterCount,
                               times.delta);
    }


    return rc;
}

RC XhciController::GetEci(void)
{
    RC rc = OK;

    if (!m_IsGetSetCap)
    {
        Printf( Tee::PriNormal,
                "The xHC doesn't support get extended property.\n");
        m_Eci = 0x0;
        return OK;
    }

    if (m_Eci == EXTND_CAP_ID_ILWALID)
    {
        XhciTrb eventTrb;
        CHECK_RC( m_pCmdRing->IssueGetExtndProperty(0,
                                                    0,
                                                    0,
                                                    0,
                                                    nullptr,
                                                    true,
                                                    0) );
        CHECK_RC(WaitForEvent(0,
                              XhciTrbTypeEvCmdComplete,
                              GetTimeout(),
                              &eventTrb));
        m_Eci = RF_VAL(XHCI_TRB_EVENT_DW2_CMD_COMPLETION_PARAM, eventTrb.DW2);
        Printf( Tee::PriNormal,
                "Enumeration of supported entended capabilities: 0x%x\n",
                m_Eci);
    }

    return rc;
}
RC XhciController::EnableBto(bool IsEnable /*=true*/)
{
    RC rc = OK;

    m_IsBto = IsEnable;
    m_BusErrCount = 0;

    return rc;
}

UINT32 XhciController::GetBusErrCount()
{
    return m_BusErrCount;
}

RC XhciController::DisablePorts(vector<UINT32> Ports)
{
    RC rc = OK;
    UINT32 val = 0;

    for (const auto& port : Ports)
    {
        if (port <= m_MaxPorts)
        {
            CHECK_RC(RegRd(XHCI_REG_PORTSC(port), &val));
            val &= ~XHCI_REG_PORTSC_WORKING_MASK;
            // set PED to 1
            val = FLD_SET_RF_NUM(XHCI_REG_PORTSC_PED, 1, val);
            Printf(Tee::PriLow, "Write 0x%08x to PortSc of port %d \n", val, port);
            CHECK_RC(RegWr(XHCI_REG_PORTSC(port), val));
        }
    }

    return rc;
}

RC XhciController::SetLowPowerMode(UINT32 Port, UINT08 LowPwrMode)
{
    LOG_ENT();
    RC rc = OK;
    switch (LowPwrMode)
    {
        case LOWPOWER_MODE_U0_Idle:
            // then wake to U0
            CHECK_RC(PortPLS(Port, nullptr, XHCI_REG_PORTSC_PLS_VAL_U0));
            // wait PLS become U0
            CHECK_RC(WaitPLS(Port, XHCI_REG_PORTSC_PLS_VAL_U0, 2000));
            break;
        case LOWPOWER_MODE_U1:
            CHECK_RC(SetU1U2Timeout(Port, 127, 0));
            CHECK_RC(WaitPLS(Port, XHCI_REG_PORTSC_PLS_VAL_U1, 100));
            break;
        case LOWPOWER_MODE_U2:
            CHECK_RC(SetU1U2Timeout(Port, 0, 128));
            CHECK_RC(WaitPLS(Port, XHCI_REG_PORTSC_PLS_VAL_U2, 300));
            break;
        case LOWPOWER_MODE_U3:
            CHECK_RC(SetU1U2Timeout(Port, 0, 0)); //Clear U1, U2 timeout
            //Go back to U0
            CHECK_RC(PortPLS(Port, nullptr, XHCI_REG_PORTSC_PLS_VAL_U0));
            Tasker::Sleep(10);
            CHECK_RC(PortPLS(Port, nullptr, XHCI_REG_PORTSC_PLS_VAL_U3));
            CHECK_RC(WaitPLS(Port, XHCI_REG_PORTSC_PLS_VAL_U3, 2000));
            // Stay in U3 for 10ms
            Tasker::Sleep(10);
            break;
        case LOWPOWER_MODE_L1:
            CHECK_RC(EnableL1Status(Port));
            CHECK_RC(WaitPLS(Port, XHCI_REG_PORTSC_PLS_VAL_L1, 2000));
            break;
        case LOWPOWER_MODE_L2:
            CHECK_RC(PortPLS(Port, nullptr, XHCI_REG_PORTSC_PLS_VAL_L2));
            CHECK_RC(WaitPLS(Port, XHCI_REG_PORTSC_PLS_VAL_L2, 2000));
            break;
        default:
            Printf( Tee::PriError, "Invalid Power State (%d)!\n", LowPwrMode);
            rc = RC::BAD_PARAMETER;
            break;
    }

    LOG_EXT();
    return rc;
}

RC XhciController::Usb2ResumeDev(UINT32 Port)
{
    RC rc = OK;

    CHECK_RC(PortPLS(Port, nullptr, XHCI_REG_PORTSC_PLS_VAL_RESUME));
    CHECK_RC(WaitPLS(Port, XHCI_REG_PORTSC_PLS_VAL_RESUME, 2000));

    return rc;
}
