/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2010,2013,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/tee.h"
#include "core/include/rc.h"
#include "cheetah/include/logmacros.h"
#include "cheetah/include/tegradrf.h"
#include "device/include/memtrk.h"
#include "core/include/platform.h"
#include "cheetah/include/tegradrf.h"
#include "xhcicntx.h"

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME "XhciContext"

DeviceContext::DeviceContext(bool Is64Byte)
{
    LOG_ENT();
    m_NumEntry  = 0;
    m_IsInit    = false;
    m_Is64Byte  = Is64Byte;
    m_pData     = NULL;
    m_AlignOffset = 0;
    m_ExtraByte = 0;

    LOG_EXT();
}

/*virtual*/ DeviceContext::~DeviceContext()
{
    LOG_ENT();
    if ( m_pData )
    {
        m_pData = ((UINT08 *)m_pData) - m_AlignOffset;
        MemoryTracker::FreeBuffer(m_pData);
        m_pData = NULL;
    }
    LOG_EXT();
}

RC DeviceContext::Init(UINT32 NumEntries, UINT32 AddrBit, UINT32 BaseOffset)
{
    LOG_ENT();
    RC rc;
    if ( m_IsInit )
    {
        Printf(Tee::PriError, "Already initializzed");
        return RC::SOFTWARE_ERROR;
    }

    if ( (NumEntries < 1) || (NumEntries > 32) )
    {
        Printf(Tee::PriError, "Invalid number of entries %d, valid [1-32]", NumEntries);
    }

    m_AlignOffset = BaseOffset % Platform::GetPageSize();

    UINT32 allocSize = NumEntries * (m_Is64Byte?64:32) + m_ExtraByte + m_AlignOffset;
    CHECK_RC(MemoryTracker::AllocBufferAligned(allocSize, &m_pData, Platform::GetPageSize(), AddrBit, Memory::UC));
    Memory::Fill08(m_pData, 0, allocSize);

    m_pData = ((UINT08*) m_pData) + m_AlignOffset;
    Printf(Tee::PriLow, " Device Context Base 0x%llx(phys), %p(virt)\n", Platform::VirtualToPhysical(m_pData), m_pData);

    m_NumEntry = NumEntries;
    m_IsInit = true;
    LOG_EXT();
    return OK;
}

RC DeviceContext::ClearOut()
{
    LOG_ENT();
    Memory::Fill08(m_pData, 0, m_NumEntry * (m_Is64Byte?64:32));
    LOG_EXT();
    return OK;
}

RC DeviceContext::GetBaseAddr(PHYSADDR * ppThis)
{
    if ( !ppThis )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    if ( !m_IsInit )
    {
        Printf(Tee::PriError, "Context not initialized\n");
        return RC::WAS_NOT_INITIALIZED;
    }
    * ppThis = Platform::VirtualToPhysical(m_pData);
    if ( ( m_AlignOffset == 0 ) &&
         ( (* ppThis) & (XHCI_ALIGNMENT_DEV_CONTEXT - 1) ) )
    {
        Printf(Tee::PriError, "Bad pointer 0x%llx, must be %d byte aligned", * ppThis, XHCI_ALIGNMENT_DEV_CONTEXT);
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

RC DeviceContext::GetSpContextBase(UINT32 EndPointNum, bool IsOut, EndPContext ** ppContext)
{
    LOG_ENT();
    RC rc;
    if ( !m_IsInit )
    {
        Printf(Tee::PriError, "Context not initialized\n");
        return RC::WAS_NOT_INITIALIZED;
    }
    if ( !ppContext )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }

    UINT08 entryIndex;
    CHECK_RC(EpToDci(EndPointNum, IsOut, &entryIndex));

    if ( entryIndex >= m_NumEntry )
    {
        Printf(Tee::PriError, "Entry Index overflow, valid[1-%d]", m_NumEntry-1);
        return RC::BAD_PARAMETER;
    }

    *ppContext = (EndPContext *) ((UINT08*)m_pData + entryIndex * (m_Is64Byte?64:32) + m_ExtraByte);
    Printf(Tee::PriDebug, "  %p(virt) returned", *ppContext);
    LOG_EXT();
    return OK;
}

RC DeviceContext::GetSlotContextBase(SlotContext ** ppContext)
{
    if ( !m_IsInit )
    {
        Printf(Tee::PriError, "Context not initialized\n");
        return RC::WAS_NOT_INITIALIZED;
    }
    if ( !ppContext )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }

    *ppContext = (SlotContext *) ((UINT08*)m_pData + m_ExtraByte);
    return OK;
}

// Slot context field access functions------------------------------------------
RC DeviceContext::GetContextEntries(UINT32 * pCntxtEntries)
{
    LOG_ENT();
    RC rc;
    if ( !pCntxtEntries )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    SlotContext * pCntxt;
    CHECK_RC(GetSlotContextBase(&pCntxt));
    *pCntxtEntries = RF_VAL(XHCI_SLOT_CONTEXT_DW0_CNTXTENTRIES, MEM_RD32(&pCntxt->DW0));
    LOG_EXT();
    return OK;
}

RC DeviceContext::GetHub(bool * pIsHub)
{
    LOG_ENT();
    RC rc;
    if ( !pIsHub )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    SlotContext * pCntxt;
    CHECK_RC(GetSlotContextBase(&pCntxt));
    *pIsHub = RF_VAL(XHCI_SLOT_CONTEXT_DW0_HUB, MEM_RD32(&pCntxt->DW0)) == 1;
    LOG_EXT();
    return OK;
}

RC DeviceContext::GetUsbDevAddr(UINT08 * pAddr)
{
    LOG_ENT();
    RC rc;
    if ( !pAddr )
    {
        Printf(Tee::PriError, "USB address Null Pointer");
        return RC::BAD_PARAMETER;
    }
    SlotContext * pCntxt;
    CHECK_RC(GetSlotContextBase(&pCntxt));
    *pAddr = RF_VAL(XHCI_SLOT_CONTEXT_DW3_USB_ADDR, MEM_RD32(&pCntxt->DW3));
    LOG_EXT();
    return OK;
}

RC DeviceContext::GetSpeed(UINT08 * pSpeed)
{
    LOG_ENT();
    RC rc;
    if ( !pSpeed )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    SlotContext * pCntxt;
    CHECK_RC(GetSlotContextBase(&pCntxt));
    *pSpeed = RF_VAL(XHCI_SLOT_CONTEXT_DW0_SPEED, MEM_RD32(&pCntxt->DW0));
    LOG_EXT();
    return OK;
}

RC DeviceContext::GetRouteString(UINT32 * pRouteString)
{
    LOG_ENT();
    RC rc;
    if ( !pRouteString )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    SlotContext * pCntxt;
    CHECK_RC(GetSlotContextBase(&pCntxt));
    *pRouteString = RF_VAL(XHCI_SLOT_CONTEXT_DW0_ROUTE_STRING, MEM_RD32(&pCntxt->DW0));
    LOG_EXT();
    return OK;
}

RC DeviceContext::GetRootHubPort(UINT08 * pPort)
{
    LOG_ENT();
    RC rc;
    if ( !pPort )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    SlotContext * pCntxt;
    CHECK_RC(GetSlotContextBase(&pCntxt));
    *pPort = RF_VAL(XHCI_SLOT_CONTEXT_DW1_ROOT_PORT_NUM, MEM_RD32(&pCntxt->DW1));
    LOG_EXT();
    return OK;
}

// Endpoint context field access functions------------------------------------------
RC DeviceContext::GetMps(UINT08 EpNum, bool IsOut, UINT32 * pMps)
{
    LOG_ENT();
    RC rc;
    if ( !pMps )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    EndPContext * pCntxt;
    CHECK_RC(GetSpContextBase(EpNum, IsOut, &pCntxt));
    *pMps = RF_VAL(XHCI_EP_CONTEXT_DW1_MPS, MEM_RD32(&pCntxt->DW1));
    LOG_EXT();
    return OK;
}

RC DeviceContext::GetRunningEps(vector<UINT08> * pvEp)
{
    RC rc;
    if ( !pvEp )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    pvEp->clear();
    UINT08 epNum;
    bool isOut;

    UINT32 numEntries = 0;
    CHECK_RC(GetContextEntries(&numEntries));
    UINT08 epState;
    for ( UINT08 dci = 1; dci <= numEntries; dci++ )
    {
        CHECK_RC(DciToEp(dci, &epNum, &isOut));
        CHECK_RC(GetEpState(epNum, isOut, &epState));
        if ( epState == EP_STATE_RUNNING )
        {
            pvEp->push_back(dci);
        }
    }
    return OK;
}

RC DeviceContext::GetEpState(UINT08 EpNum, bool IsOut, UINT08 * pEpState)
{
    LOG_ENT();
    RC rc;
    if ( !pEpState )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    EndPContext * pCntxt;
    CHECK_RC(GetSpContextBase(EpNum, IsOut, &pCntxt));
    *pEpState = RF_VAL(XHCI_EP_CONTEXT_DW0_EP_STATE, MEM_RD32(&pCntxt->DW0));
    LOG_EXT();
    return OK;
}

RC DeviceContext::GetEpRawData(UINT08 EndpNum, bool IsOut, EndPContext * pEpContext)
{
    if ( !pEpContext )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    RC rc;
    EndPContext * pTmpContext;
    CHECK_RC(GetSpContextBase(EndpNum, IsOut, &pTmpContext));

    MEM_WR32(&pEpContext->DW0, MEM_RD32(&pTmpContext->DW0));
    MEM_WR32(&pEpContext->DW1, MEM_RD32(&pTmpContext->DW1));
    MEM_WR32(&pEpContext->DW2, MEM_RD32(&pTmpContext->DW2));
    MEM_WR32(&pEpContext->DW3, MEM_RD32(&pTmpContext->DW3));
    MEM_WR32(&pEpContext->DW4, MEM_RD32(&pTmpContext->DW4));
    MEM_WR32(&pEpContext->RSVD0, MEM_RD32(&pTmpContext->RSVD0));
    MEM_WR32(&pEpContext->RSVD1, MEM_RD32(&pTmpContext->RSVD1));
    MEM_WR32(&pEpContext->RSVD2, MEM_RD32(&pTmpContext->RSVD2));

    return OK;
}

RC DeviceContext::PrintInfo(Tee::Priority PriRaw, Tee::Priority PriInfo)
{
    RC rc;
    if ( !m_pData )
    {
        Printf(Tee::PriError, "Context hasn't been allocate yet");
        return RC::BAD_PARAMETER;
    }

    SlotContext * pSlotCntxt = (SlotContext*) ((UINT08 *)m_pData + m_ExtraByte);
    const char * strSlotState[] = {"Disabled","Default","Addressed","Configured","Reserved"};

    Printf(PriRaw, "Slot Context \n");
    Printf(PriRaw, " 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%x, 0x%x, 0x%x, 0x%x \n",
           MEM_RD32(&pSlotCntxt->DW0),MEM_RD32(&pSlotCntxt->DW1),
           MEM_RD32(&pSlotCntxt->DW2),MEM_RD32(&pSlotCntxt->DW3),
           MEM_RD32(&pSlotCntxt->RSVD0),MEM_RD32(&pSlotCntxt->RSVD1),
           MEM_RD32(&pSlotCntxt->RSVD2),MEM_RD32(&pSlotCntxt->RSVD3));
    Printf(PriInfo,
           "  Route String 0x%x, Speed %d, Multi-TT %d, Hub %d, Context Entries %d \n"
           "  Max Exit Latency %dMs, Root Hub Port Number %d, Number of ports %u \n"
           "  TT Hub Slot ID %d, TT Port Number %d, TT Think time %u, Interrupter Target %d \n"
           "  USB Device Address %d, Slot State ",
           RF_VAL(XHCI_SLOT_CONTEXT_DW0_ROUTE_STRING, MEM_RD32(&pSlotCntxt->DW0)),
           RF_VAL(XHCI_SLOT_CONTEXT_DW0_SPEED, MEM_RD32(&pSlotCntxt->DW0)),
           RF_VAL(XHCI_SLOT_CONTEXT_DW0_MTT, MEM_RD32(&pSlotCntxt->DW0)),
           RF_VAL(XHCI_SLOT_CONTEXT_DW0_HUB, MEM_RD32(&pSlotCntxt->DW0)),
           RF_VAL(XHCI_SLOT_CONTEXT_DW0_CNTXTENTRIES, MEM_RD32(&pSlotCntxt->DW0)),

           RF_VAL(XHCI_SLOT_CONTEXT_DW1_MAX_EXIT_LATENCY, MEM_RD32(&pSlotCntxt->DW1)),
           RF_VAL(XHCI_SLOT_CONTEXT_DW1_ROOT_PORT_NUM, MEM_RD32(&pSlotCntxt->DW1)),
           RF_VAL(XHCI_SLOT_CONTEXT_DW1_NUM_OF_PORT, MEM_RD32(&pSlotCntxt->DW1)),

           RF_VAL(XHCI_SLOT_CONTEXT_DW2_TT_HUB_SLOTID, MEM_RD32(&pSlotCntxt->DW2)),
           RF_VAL(XHCI_SLOT_CONTEXT_DW2_TT_PORT_NUM, MEM_RD32(&pSlotCntxt->DW2)),
           RF_VAL(XHCI_SLOT_CONTEXT_DW2_TTT, MEM_RD32(&pSlotCntxt->DW2)),
           RF_VAL(XHCI_SLOT_CONTEXT_DW2_INTERRUPTER_TARGET, MEM_RD32(&pSlotCntxt->DW2)),

           RF_VAL(XHCI_SLOT_CONTEXT_DW3_USB_ADDR, MEM_RD32(&pSlotCntxt->DW3))
          );

    UINT32 slotState = RF_VAL(XHCI_SLOT_CONTEXT_DW3_SLOT_DTATE, MEM_RD32(&pSlotCntxt->DW3));
    if ( slotState > 4 )
        slotState = 4;
    Printf(PriInfo, "%s \n", strSlotState[slotState]);

    CHECK_RC(PrintEpContext(1, PriRaw, PriInfo));
    return OK;
}

RC DeviceContext::PrintEpContext(UINT08 StartIndex,
                                 Tee::Priority PriRaw,
                                 Tee::Priority PriInfo)
{
    RC rc;
    const char * strSpState[] = {"Disabled", "Running", "Halted", "Stopped", "Reserved"};
    const char * strEpType[] = {"Invalid", "Isoch Out", "Bulk Out", "Interrupt Out", "Control","Isoch In", "Bulk In", "Interrupt In"};

    EndPContext * pEpCntxt = (EndPContext*) ( (UINT08 *)m_pData + m_ExtraByte + StartIndex*(m_Is64Byte?64:32) );
    // Print Endpoint Contexts
    Printf(PriRaw, "Endpoint Contexts \n");

    for ( UINT32 i = StartIndex; i < m_NumEntry; i++ )
    {
        if ( ( MEM_RD32(&pEpCntxt->DW0) | MEM_RD32(&pEpCntxt->DW1) | MEM_RD32(&pEpCntxt->DW2) | MEM_RD32(&pEpCntxt->DW3) | MEM_RD32(&pEpCntxt->DW4) ) != 0 )
        {
            UINT08 epNum;
            bool isDataOut;
            CHECK_RC(DciToEp(i, &epNum, &isDataOut));
            Printf(PriRaw, "Endpoint Context DCI %d: Endpoint %d Data %s", i, epNum, isDataOut?"Out":"In");
            Printf(PriRaw, " (virt:%p) \n", pEpCntxt);
            Printf(PriRaw, " 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%x, 0x%x, 0x%x \n",
                   MEM_RD32(&pEpCntxt->DW0), MEM_RD32(&pEpCntxt->DW1), MEM_RD32(&pEpCntxt->DW2),
                   MEM_RD32(&pEpCntxt->DW3), MEM_RD32(&pEpCntxt->DW4),
                   MEM_RD32(&pEpCntxt->RSVD0), MEM_RD32(&pEpCntxt->RSVD1), MEM_RD32(&pEpCntxt->RSVD2));

            UINT32 epState = RF_VAL(XHCI_EP_CONTEXT_DW0_EP_STATE, MEM_RD32(&pEpCntxt->DW0));
            if ( epState > 4 )
                epState = 4;
            Printf(PriInfo, "  EP State %s, Mult %d, MaxPStreams %d, Interval %d (%d * 125uS) \n"
                   "  Force Event %d, Err Count %d, Endpoint Type %d (%s), Max Burst Size %d \n"
                   "  Max Packet Size %d, Dequeue Cycle %d, TR Dequeue Pointer 0x%08x%08x \n"
                   "  Average TRB Length %d, Max ESIT Payload %d \n",
                   strSpState[epState],
                   RF_VAL(XHCI_EP_CONTEXT_DW0_MULT, MEM_RD32(&pEpCntxt->DW0)),
                   RF_VAL(XHCI_EP_CONTEXT_DW0_MAX_PSTREAMS, MEM_RD32(&pEpCntxt->DW0)),
                   RF_VAL(XHCI_EP_CONTEXT_DW0_INTERVAL, MEM_RD32(&pEpCntxt->DW0)), 1<<RF_VAL(XHCI_EP_CONTEXT_DW0_INTERVAL, MEM_RD32(&pEpCntxt->DW0)),

                   RF_VAL(XHCI_EP_CONTEXT_DW1_FE, MEM_RD32(&pEpCntxt->DW1)),
                   RF_VAL(XHCI_EP_CONTEXT_DW1_CREE, MEM_RD32(&pEpCntxt->DW1)),
                   RF_VAL(XHCI_EP_CONTEXT_DW1_EP_TYPE, MEM_RD32(&pEpCntxt->DW1)),strEpType[RF_VAL(XHCI_EP_CONTEXT_DW1_EP_TYPE, MEM_RD32(&pEpCntxt->DW1))],
                   RF_VAL(XHCI_EP_CONTEXT_DW1_MAX_BURST_SIZE, MEM_RD32(&pEpCntxt->DW1)),

                   RF_VAL(XHCI_EP_CONTEXT_DW1_MPS, MEM_RD32(&pEpCntxt->DW1)),
                   RF_VAL(XHCI_EP_CONTEXT_DW2_DCS, MEM_RD32(&pEpCntxt->DW2)),
                   RF_VAL(XHCI_EP_CONTEXT_DW3_DEQ_POINTER_HI, MEM_RD32(&pEpCntxt->DW3)),
                   RF_VAL(XHCI_EP_CONTEXT_DW2_DEQ_POINTER_LO, MEM_RD32(&pEpCntxt->DW2)) << (0?XHCI_EP_CONTEXT_DW2_DEQ_POINTER_LO),

                   RF_VAL(XHCI_EP_CONTEXT_DW4_AVERAGE_TRB_LENGTH, MEM_RD32(&pEpCntxt->DW4)),
                   RF_VAL(XHCI_EP_CONTEXT_DW4_MAX_ESIT_PAYLOAD, MEM_RD32(&pEpCntxt->DW4))
                  );
        }
        pEpCntxt = (EndPContext *)((UINT08 *) pEpCntxt + (m_Is64Byte?64:32));
    }
    Printf(PriRaw,"\n");
    return OK;
}
RC DeviceContext::DciToEp(UINT08 Dci, UINT08 * pEp, bool * pIsDataOut)
{
    if (  Dci > 31 )
    {
        Printf(Tee::PriError, "Ilwaid DCI %d, valid [0-31]", Dci);
        return RC::BAD_PARAMETER;
    }
    if ( !pEp || !pIsDataOut )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    *pEp = Dci / 2;
    *pIsDataOut = (Dci - (*pEp) * 2)==0;
    Printf(Tee::PriDebug, "Dci %d -> Ep %d, Data %s", Dci, *pEp, (*pIsDataOut)?"Out":"In");
    LOG_EXT();
    return OK;
}

RC DeviceContext::EpToDci(UINT08 Ep, bool IsDataOut, UINT08 * pDci, bool IsEp0Dir)
{
    if ( Ep > 15 )
    {
        Printf(Tee::PriError, "Ilwaid Endpoint %d, valid [0-15]", Ep);
        return RC::BAD_PARAMETER;
    }
    if ( !pDci )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    * pDci = Ep * 2 + (IsDataOut?0:1);
    if (Ep == 0)
    {
        if ( (!IsEp0Dir) )
        {
            * pDci = 1;
        }
        else
        {   // Device Mode use only DCI 0 for Control endpoint
            * pDci = 0;
        }
    }
    Printf(Tee::PriDebug, "Ep %d, Data %s -> Dci %d", Ep, IsDataOut?"Out":"In", *pDci);
    LOG_EXT();
    return OK;
}

RC DeviceContext::ToVector(vector<UINT32> * pvData)
{
    if ( !pvData )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    if ( m_ExtraByte % 4 )
    {
        Printf(Tee::PriError, "Context exter Byte size not DW aligned, padding");
        return RC::BAD_PARAMETER;   // this shouldn't happen
    }

    UINT32 * pData32 = (UINT32 *)m_pData;
    for ( UINT32 i = 0; i < (m_ExtraByte/4); i++ )
    {
        pvData->push_back((UINT32) MEM_RD32(pData32));
        pData32++;
    }

    for ( UINT32 i = 0; i < m_NumEntry; i++ )
    {
        for ( UINT32 j = 0; j < ((m_Is64Byte?64U:32U)/4U); j++ )
        {
            pvData->push_back((UINT32) MEM_RD32(pData32));
            pData32++;
        }
    }

    return OK;
}

RC DeviceContext::FromVector(vector<UINT32> * pvData, bool IsForceFullCopy, bool IsSkipInputControl)
{
    if (!m_IsInit)
    {
        Printf(Tee::PriError, "Context hasn't been init yet\n");
        return RC::SOFTWARE_ERROR;
    }
    if ( !pvData || !pvData->size())
    {
        Printf(Tee::PriError, "Null vector");
        return RC::BAD_PARAMETER;
    }
    if ( m_ExtraByte % 4 )
    {
        Printf(Tee::PriError, "Context extra Byte size not DW aligned");
        return RC::BAD_PARAMETER;   // this shouldn't happen
    }

    UINT32 * pData32 = (UINT32 *)m_pData;
    UINT32 index = 0;
    for ( UINT32 i = 0; i < (m_ExtraByte/4); i++ )
    {
        if ( !IsSkipInputControl )
        {   // advance the Write Pointer but don't touch the Read Pointer
            MEM_WR32(pData32, (*pvData)[index]);
            index++;
        }
        pData32++;
        if ( index >= pvData->size() )
        {
            return OK;
        }
    }

    for ( UINT32 i = 0; i < m_NumEntry; i++ )
    {
        for ( UINT32 j = 0; j < ((m_Is64Byte?64U:32U)/4U); j++ )
        {
            if ( ( IsForceFullCopy )
                 || ( 0 == i )                  // slot context
                 || ( (2 != j) && (3 != j) ) )  // enpoint context
            {// in any case, we should not overwrite TR Dequeue Pointer and DCS for all Endpoint Contexts
                MEM_WR32(pData32, (*pvData)[index]);
            }
            index++;
            pData32++;
            if ( index >= pvData->size() )
            {
                return OK;
            }
        }
    }

    return OK;
}

// Input Context
//==============================================================================
InputContext::InputContext(bool Is64Byte):DeviceContext(Is64Byte)
{
    LOG_ENT();
    m_ExtraByte = Is64Byte?64:32;
    LOG_EXT();
}

/*virtual*/ InputContext::~InputContext()
{
    LOG_ENT();
    LOG_EXT();
}

RC InputContext::PrintInfo(Tee::Priority PriRaw, Tee::Priority PriInfo)
{
    RC rc;
    UINT32 * pData32 = (UINT32 *) m_pData;
    Printf(PriRaw, "Input Context \n");
    for (UINT32 i = 0; i < m_ExtraByte / 4; i++)
    {
        Printf(PriRaw, " 0x%x", MEM_RD32(&pData32[i]));
    }
    Printf(PriRaw, " \n");
    Printf(PriInfo, "  Drop Context flags: 0x%08x, Add Context flags: 0x%08x \n", MEM_RD32(pData32), MEM_RD32(pData32+1));

    CHECK_RC(DeviceContext::PrintInfo(PriRaw, PriInfo));
    return OK;
}

RC InputContext::Init(UINT32 BaseOffset)
{
    return DeviceContext::Init(32, 32, BaseOffset);
}

RC InputContext::GetInpCtrlContextBase(UINT32 ** ppData)
{
    if ( !m_IsInit )
    {
        Printf(Tee::PriError, "Input Context not initialized\n");
        return RC::WAS_NOT_INITIALIZED;
    }
    if ( !ppData )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }

    *ppData = (UINT32 *) m_pData;
    return OK;
}

RC InputContext::SetInuputControl(UINT32 DropFlags, UINT32 AddFlags)
{
    LOG_ENT();
    RC rc;

    if ( DropFlags & 0x3 )
        Printf(Tee::PriWarn, "Slot context and EP0 context can not be dropped");

    UINT32 * pData;
    CHECK_RC(GetInpCtrlContextBase(&pData));
    MEM_WR32(pData, DropFlags);
    MEM_WR32(pData + 1, AddFlags);

    LOG_EXT();
    return OK;
}

// Slot context field setting functions-----------------------------------------
RC InputContext::SetContextEntries(UINT32 CntxtEntries)
{
    LOG_ENT();
    RC rc;

    if ( CntxtEntries > 31 )
    {
        Printf(Tee::PriError, "Invalid num of entries %d, valid [1-31]\n", CntxtEntries);
        return RC::BAD_PARAMETER;
    }
    SlotContext * pCntxt;
    CHECK_RC(GetSlotContextBase(&pCntxt));
    MEM_WR32(&pCntxt->DW0, FLD_SET_RF_NUM(XHCI_SLOT_CONTEXT_DW0_CNTXTENTRIES, CntxtEntries, MEM_RD32(&pCntxt->DW0)));
    LOG_EXT();
    return OK;
}

RC InputContext::SetSpeed(UINT08 Speed)
{
    LOG_ENT();
    RC rc;

    SlotContext * pCntxt;
    CHECK_RC(GetSlotContextBase(&pCntxt));
    MEM_WR32(&pCntxt->DW0, FLD_SET_RF_NUM(XHCI_SLOT_CONTEXT_DW0_SPEED, Speed, MEM_RD32(&pCntxt->DW0)));
    LOG_EXT();
    return OK;
}

RC InputContext::SetRootHubPort(UINT08 PortNum)
{
    LOG_ENT();
    RC rc;

    SlotContext * pCntxt;
    CHECK_RC(GetSlotContextBase(&pCntxt));
    MEM_WR32(&pCntxt->DW1, FLD_SET_RF_NUM(XHCI_SLOT_CONTEXT_DW1_ROOT_PORT_NUM, PortNum, MEM_RD32(&pCntxt->DW1)));
    LOG_EXT();
    return OK;
}

RC InputContext::SetHubBit(bool IsHub)
{
    LOG_ENT();
    RC rc;

    SlotContext * pCntxt;
    CHECK_RC(GetSlotContextBase(&pCntxt));
    MEM_WR32(&pCntxt->DW0, FLD_SET_RF_NUM(XHCI_SLOT_CONTEXT_DW0_HUB, IsHub?1:0, MEM_RD32(&pCntxt->DW0)));
    LOG_EXT();
    return OK;
}

RC InputContext::SetMaxExitLatency(UINT16 MaxLatMs)
{
    LOG_ENT();
    RC rc;

    SlotContext * pCntxt;
    CHECK_RC(GetSlotContextBase(&pCntxt));
    MEM_WR32(&pCntxt->DW1, FLD_SET_RF_NUM(XHCI_SLOT_CONTEXT_DW1_MAX_EXIT_LATENCY, MaxLatMs, MEM_RD32(&pCntxt->DW1)));
    LOG_EXT();
    return OK;
}

RC InputContext::SetIntrTarget(UINT16 IntrTarget)
{
    LOG_ENT();
    RC rc;

    SlotContext * pCntxt;
    CHECK_RC(GetSlotContextBase(&pCntxt));
    MEM_WR32(&pCntxt->DW2, FLD_SET_RF_NUM(XHCI_SLOT_CONTEXT_DW2_INTERRUPTER_TARGET, IntrTarget, MEM_RD32(&pCntxt->DW2)));
    LOG_EXT();
    return OK;
}

RC InputContext::SetTtt(UINT08 Ttt)
{
    LOG_ENT();
    RC rc;
    if ( Ttt > 0x3 )
    {
        Printf(Tee::PriError, "Incorrect TTT value %u", Ttt);
        return RC::BAD_PARAMETER;
    }

    SlotContext * pCntxt;
    CHECK_RC(GetSlotContextBase(&pCntxt));
    MEM_WR32(&pCntxt->DW2, FLD_SET_RF_NUM(XHCI_SLOT_CONTEXT_DW2_TTT, Ttt, MEM_RD32(&pCntxt->DW2)));
    LOG_EXT();
    return OK;
}

RC InputContext::SetMtt(bool Mtt)
{
    LOG_ENT();
    RC rc;

    SlotContext * pCntxt;
    CHECK_RC(GetSlotContextBase(&pCntxt));
    MEM_WR32(&pCntxt->DW0, FLD_SET_RF_NUM(XHCI_SLOT_CONTEXT_DW0_MTT, Mtt?1:0, MEM_RD32(&pCntxt->DW0)));
    LOG_EXT();
    return OK;
}

RC InputContext::SetNumOfPorts(UINT08 NumPort)
{
    LOG_ENT();
    RC rc;

    SlotContext * pCntxt;
    CHECK_RC(GetSlotContextBase(&pCntxt));
    MEM_WR32(&pCntxt->DW1, FLD_SET_RF_NUM(XHCI_SLOT_CONTEXT_DW1_NUM_OF_PORT, NumPort, MEM_RD32(&pCntxt->DW1)));
    LOG_EXT();
    return OK;
}

RC InputContext::SetRouteString(UINT32 RouteString)
{
    LOG_ENT();
    RC rc;

    if ( RouteString & 0xfff00000 )
    {
        Printf(Tee::PriError, "Invalid Route String 0x%08x\n", RouteString);
        return RC::BAD_PARAMETER;
    }
    SlotContext * pCntxt;
    CHECK_RC(GetSlotContextBase(&pCntxt));
    MEM_WR32(&pCntxt->DW0, FLD_SET_RF_NUM(XHCI_SLOT_CONTEXT_DW0_ROUTE_STRING, RouteString, MEM_RD32(&pCntxt->DW0)));

    LOG_EXT();
    return OK;
}

// Endpoint context field setting functions-------------------------------------
RC InputContext::InitEpContext(UINT32 EpNum, bool IsOut,
                               UINT08 Type,
                               UINT08 Interval,
                               UINT08 MaxBurstSize,
                               UINT08 MaxPStreams,
                               UINT08 Mult,
                               UINT08 Cerr,
                               UINT08 Fe,
                               bool isLinearStreamArray,
                               bool IsHostInitiateDisable)
{
    LOG_ENT();
    RC rc;

    EndPContext * pCntxt;
    CHECK_RC(GetSpContextBase(EpNum, IsOut, &pCntxt));
    UINT32 data32;

    data32 = MEM_RD32(&pCntxt->DW0);
    data32 = FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW0_INTERVAL, Interval, data32);
    data32 = FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW0_LSA, isLinearStreamArray?1:0, data32);
    data32 = FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW0_MAX_PSTREAMS, MaxPStreams, data32);
    data32 = FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW0_MULT, Mult, data32);
    MEM_WR32(&pCntxt->DW0, data32);

    data32 = MEM_RD32(&pCntxt->DW1);
    data32 = FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW1_MAX_BURST_SIZE, MaxBurstSize, data32);
    data32 = FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW1_HID, IsHostInitiateDisable?1:0, data32);
    data32 = FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW1_EP_TYPE, Type, data32);
    data32 = FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW1_CREE, Cerr, data32);
    data32 = FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW1_FE, Fe, data32);
    MEM_WR32(&pCntxt->DW1, data32);

    LOG_EXT();
    return OK;
}

RC InputContext::SetEpMps(UINT32 EpNum, bool IsOut, UINT32 Mps)
{
    LOG_ENT();
    RC rc;

    EndPContext * pCntxt;
    CHECK_RC(GetSpContextBase(EpNum, IsOut, &pCntxt));
    MEM_WR32(&pCntxt->DW1, FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW1_MPS, Mps, MEM_RD32(&pCntxt->DW1)));

    LOG_EXT();
    return OK;
}

RC InputContext::SetEpMult(UINT32 EpNum, bool IsOut, UINT32 Mult)
{
    LOG_ENT();
    RC rc;

    EndPContext * pCntxt;
    CHECK_RC(GetSpContextBase(EpNum, IsOut, &pCntxt));
    MEM_WR32(&pCntxt->DW0, FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW0_MULT, Mult, MEM_RD32(&pCntxt->DW0)));

    LOG_EXT();
    return OK;
}

RC InputContext::SetEpMaxBurstSize(UINT32 EpNum, bool IsOut, UINT32 MaxBurstSize)
{
    LOG_ENT();
    RC rc;

    EndPContext * pCntxt;
    CHECK_RC(GetSpContextBase(EpNum, IsOut, &pCntxt));
    MEM_WR32(&pCntxt->DW1, FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW1_MAX_BURST_SIZE, MaxBurstSize, MEM_RD32(&pCntxt->DW1)));

    LOG_EXT();
    return OK;
}

RC InputContext::SetEpDeqPtr(UINT32 EpNum, bool IsOut, PHYSADDR pPa)
{
    LOG_ENT();
    RC rc;

    EndPContext * pCntxt;
    CHECK_RC(GetSpContextBase(EpNum, IsOut, &pCntxt));
    MEM_WR32(&pCntxt->DW2, FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW2_DEQ_POINTER_LO, pPa>>(0?XHCI_EP_CONTEXT_DW2_DEQ_POINTER_LO), MEM_RD32(&pCntxt->DW2)));
    MEM_WR32(&pCntxt->DW3, pPa >> 32);

    LOG_EXT();
    return OK;
}

RC InputContext::SetEpDcs(UINT32 EpNum, bool IsOut, bool Dcs)
{
    LOG_ENT();
    RC rc;

    EndPContext * pCntxt;
    CHECK_RC(GetSpContextBase(EpNum, IsOut, &pCntxt));
    MEM_WR32(&pCntxt->DW2, FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW2_DCS, Dcs?1:0, MEM_RD32(&pCntxt->DW2)));

    LOG_EXT();
    return OK;
}

RC InputContext::SetEpBwParams(UINT32 EpNum, bool IsOut, UINT16 AverageTrbLength, UINT16 MaxESITPayload)
{
    LOG_ENT();
    RC rc;

    EndPContext * pCntxt;
    CHECK_RC(GetSpContextBase(EpNum, IsOut, &pCntxt));

    MEM_WR32(&pCntxt->DW4, FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW4_AVERAGE_TRB_LENGTH, AverageTrbLength, MEM_RD32(&pCntxt->DW4)));
    MEM_WR32(&pCntxt->DW4, FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW4_MAX_ESIT_PAYLOAD, MaxESITPayload, MEM_RD32(&pCntxt->DW4)));

    LOG_EXT();
    return OK;
}

// Device Mode Endpoint Context
//==============================================================================
DevModeEpContexts::DevModeEpContexts():DeviceContext(true)   // IAS indicates we will always use 64Byte format
{
    LOG_ENT();
    LOG_EXT();
}

DevModeEpContexts::~DevModeEpContexts()
{
    LOG_ENT();
    LOG_EXT();
}

RC DevModeEpContexts::BindXferRing(UINT08 EndpNum, bool IsOut, PHYSADDR pXferRing)
{
    LOG_ENT();
    RC rc;

    EndPContext * pCntxt;
    CHECK_RC(GetSpContextBase(EndpNum, IsOut, &pCntxt));

    MEM_WR32(&pCntxt->DW2, FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW2_DEQ_POINTER_LO, pXferRing>>(0?XHCI_EP_CONTEXT_DW2_DEQ_POINTER_LO), MEM_RD32(&pCntxt->DW2)));
    MEM_WR32(&pCntxt->DW3, pXferRing >> 32);

    LOG_EXT();
    return OK;
}

RC DevModeEpContexts::GetSpContextBase(UINT32 EndpNum, bool IsOut, EndPContext ** ppContext)
{
    LOG_ENT();
    RC rc;
    if ( !m_IsInit )
    {
        Printf(Tee::PriError, "Context not initialized\n");
        return RC::WAS_NOT_INITIALIZED;
    }
    if ( !ppContext )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    if ( EndpNum > 15 )
    {
        Printf(Tee::PriError, "Ilwaid Endpoint %d, valid [0-15]", EndpNum);
        return RC::BAD_PARAMETER;
    }

    UINT08 entryIndex;
    CHECK_RC(EpToDci(EndpNum, IsOut, &entryIndex, true));

    if ( entryIndex >= m_NumEntry )
    {
        Printf(Tee::PriError, "Entry Index overflow, valid[0-%d]", m_NumEntry-1);
        return RC::BAD_PARAMETER;
    }

    *ppContext = (EndPContext *) ((UINT08*)m_pData + entryIndex * (m_Is64Byte?64:32));
    Printf(Tee::PriDebug, "  %p(virt) returned for entry %d ", *ppContext, entryIndex);
    LOG_EXT();
    return OK;
}

RC DevModeEpContexts::InitEpContext(UINT32 EpNum, bool IsOut,
                                    UINT08 Type,
                                    UINT08 EpState,
                                    UINT32 Mps,
                                    bool   Dcs,
                                    UINT08 Interval,
                                    UINT08 MaxBurstSize,
                                    UINT08 MaxPStreams,
                                    UINT08 Mult,
                                    UINT08 Cerr,
                                    UINT08 Fe,
                                    bool IsLinearStreamArray,
                                    bool IsHostInitiateDisable)
{
    LOG_ENT();
    RC rc;

    EndPContext * pCntxt;
    CHECK_RC(GetSpContextBase(EpNum, IsOut, &pCntxt));
    UINT32 data32;

    data32 = MEM_RD32(&pCntxt->DW0);
    data32 = FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW0_LSA, IsLinearStreamArray?1:0, data32);
    data32 = FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW0_EP_STATE, EpState, data32);
    data32 = FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW0_INTERVAL, Interval, data32);
    data32 = FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW0_MAX_PSTREAMS, MaxPStreams, data32);
    data32 = FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW0_MULT, Mult, data32);
    MEM_WR32(&pCntxt->DW0, data32);

    data32 = MEM_RD32(&pCntxt->DW1);
    data32 = FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW1_EP_TYPE, Type, data32);
    data32 = FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW1_MAX_BURST_SIZE, MaxBurstSize, data32);
    data32 = FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW1_HID, IsHostInitiateDisable?1:0, data32);
    data32 = FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW1_CREE, Cerr, data32);
    data32 = FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW1_FE, Fe, data32);
    data32 = FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW1_MPS, Mps, data32);
    MEM_WR32(&pCntxt->DW1, data32);

    data32 = MEM_RD32(&pCntxt->DW2);
    data32 = FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW2_DCS, Dcs?1:0, data32);
    MEM_WR32(&pCntxt->DW2, data32);

    LOG_EXT();
    return OK;
}

RC DevModeEpContexts::SetEpState(UINT32 EpNum, bool IsOut, UINT08 EpState)
{
    LOG_ENT();
    RC rc;

    EndPContext * pCntxt;
    CHECK_RC(GetSpContextBase(EpNum, IsOut, &pCntxt));

    UINT32 data32 = MEM_RD32(&pCntxt->DW0);
    data32 = FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW0_EP_STATE, EpState, data32);
    MEM_WR32(&pCntxt->DW0, data32);

    LOG_EXT();
    return OK;
}

RC DevModeEpContexts::SetSeqNum(UINT32 EpNum, bool IsOut, UINT08 SeqNum)
{
    LOG_ENT();
    RC rc;

    EndPContext * pCntxt;
    CHECK_RC(GetSpContextBase(EpNum, IsOut, &pCntxt));

    UINT32 data32 = MEM_RD32(&pCntxt->RSVD0);
    data32 = FLD_SET_RF_NUM(XHCI_EP_CONTEXT_DW5_SQR_NUM, SeqNum, data32);
    MEM_WR32(&pCntxt->RSVD0, data32);
    Printf(Tee::PriDebug, "SetSeqNum = %d, written 0x%x \n", SeqNum, data32);

    LOG_EXT();
    return OK;
}

// Device Context Base Address Array
//==============================================================================
DeviceContextArray::DeviceContextArray()
{
    LOG_ENT();

    m_pData = NULL;
    for ( UINT32 i = 0; i<XHCI_MAX_SIZE_DEV_CONTEXT_ARRAY; i++ )
    {
        m_DevCntxtArray[i] = NULL;
    }

    LOG_EXT();
}

/*virtual*/ DeviceContextArray::~DeviceContextArray()
{
    LOG_ENT();
    // release all the Device Context Objects
    for ( UINT32 i = 0; i<XHCI_MAX_SIZE_DEV_CONTEXT_ARRAY; i++ )
    {
        if ( m_DevCntxtArray[i] )
        {
            delete m_DevCntxtArray[i];
            m_DevCntxtArray[i] = NULL;
        }
    }
    // release this
    if ( m_pData )
    {
        MemoryTracker::FreeBuffer(m_pData);
        m_pData = NULL;
    }
    LOG_EXT();
}

RC DeviceContextArray::Init(UINT32 AddrBit)
{
    RC rc;
    // allocate Device Context Base Address Array
    CHECK_RC(MemoryTracker::AllocBufferAligned(XHCI_MAX_SIZE_DEV_CONTEXT_ARRAY * XHCI_LENGTH_DCBAA_ENTRY, &m_pData, XHCI_ALIGNMENT_DEV_CONTEXT_ARRAY, AddrBit, Memory::UC));
    Memory::Fill32(m_pData, 0, XHCI_MAX_SIZE_DEV_CONTEXT_ARRAY * XHCI_LENGTH_DCBAA_ENTRY / 4);
    return OK;
}

RC DeviceContextArray::GetBaseAddr(PHYSADDR * ppArray)
{
    if ( !ppArray )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    if ( !m_pData )
    {
        Printf(Tee::PriError, "Stream Context Array not initialized yet");
        return RC::WAS_NOT_INITIALIZED;
    }

    * ppArray = Platform::VirtualToPhysical(m_pData);
    if ( (* ppArray) & (XHCI_ALIGNMENT_DEV_CONTEXT_ARRAY - 1) )
    {
        Printf(Tee::PriError, "Bad pointer 0x%llx, must be %d byte aligned\n", * ppArray, XHCI_ALIGNMENT_DEV_CONTEXT_ARRAY);
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

RC DeviceContextArray::DeleteEntry(UINT32 SlotId)
{
    LOG_EXT();

    if ( SlotId >= XHCI_MAX_SIZE_DEV_CONTEXT_ARRAY )    // slotid = [0- 255]
    {
        Printf(Tee::PriError, "Invalid SlotId %d, overflow\n", SlotId);
        return RC::BAD_PARAMETER;
    }
    if ( !m_DevCntxtArray[SlotId] || SlotId == 0 )
    {
        Printf(Tee::PriError, "Entry %d is empty, can't be removed", SlotId);
        return RC::SOFTWARE_ERROR;
    }
    // delete the object pointer
    delete m_DevCntxtArray[SlotId];
    m_DevCntxtArray[SlotId] = NULL;

    // update the device context base array
    UINT32 * pData32 = (UINT32*)((UINT08*)m_pData + SlotId * XHCI_LENGTH_DCBAA_ENTRY);
    MEM_WR32(pData32, 0);
    MEM_WR32(pData32+1, 0);

    return OK;
}

RC DeviceContextArray::GetActiveSlots(vector<UINT08> * pvSlots)
{
    if ( !pvSlots )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }

    pvSlots->clear();
    for ( UINT32 i = 0; i < XHCI_MAX_SIZE_DEV_CONTEXT_ARRAY; i++ )
    {
        if ( m_DevCntxtArray[i] )
        {
            pvSlots->push_back(i);
        }
    }
    return OK;
}

RC DeviceContextArray::GetEntry(UINT32 SlotId, DeviceContext ** ppDevContext, bool IsSilent)
{
    LOG_ENT();

    if ( SlotId >= XHCI_MAX_SIZE_DEV_CONTEXT_ARRAY )
    {
        Printf(Tee::PriError, "Invalid SlotId %d\n", SlotId);
        return RC::BAD_PARAMETER;
    }
    if (( !m_DevCntxtArray[SlotId]) && (!IsSilent))
    {
        Printf(Tee::PriError, "Entry %d is empty\n", SlotId);
        return RC::BAD_PARAMETER;
    }
    if (ppDevContext)
    {
        * ppDevContext = m_DevCntxtArray[SlotId];
    }

    LOG_EXT();
    return OK;
}

RC DeviceContextArray::SetEntry(UINT32 SlotId, DeviceContext * pDevContext)
{
    //Note: the first entry (Slot ID = 0) in the Device Context Base Address Array is always RESERVED and shall be set to 0.
    LOG_ENT(); Printf(Tee::PriDebug,"(SlotId = %d, pDevContext %p(virt))\n", SlotId, pDevContext);
    RC rc;
    if ( SlotId >= XHCI_MAX_SIZE_DEV_CONTEXT_ARRAY )
    {
        Printf(Tee::PriError, "Invalid SlotId %d, Total slots %d\n", SlotId, XHCI_MAX_SIZE_DEV_CONTEXT_ARRAY);
        return RC::BAD_PARAMETER;
    }
    if ( SlotId == 0 )
    {
        Printf(Tee::PriError, "Slot ID 0 is reserved\n");
        return RC::SOFTWARE_ERROR;
    }
    if (m_DevCntxtArray[SlotId])
    {
        Printf(Tee::PriError, "Slot %d is oclwpied. Release it first!\n", SlotId);
        return RC::SOFTWARE_ERROR;
    }
    // preserve the original object
    m_DevCntxtArray[SlotId] = pDevContext;
    // set the address to Device Context data structure
    UINT32 * pData32 = (UINT32*)((UINT08*)m_pData + SlotId * XHCI_LENGTH_DCBAA_ENTRY);

    PHYSADDR pPa;
    CHECK_RC(pDevContext->GetBaseAddr(&pPa));

    MEM_WR32(pData32, (UINT32)pPa);
    pPa = pPa >> 32;
    MEM_WR32(pData32 + 1, (UINT32)pPa);
    Printf(Tee::PriLow, "  0x%08x%08x --> Device Context Array Slot %d\n", MEM_RD32(pData32+1), MEM_RD32(pData32), SlotId);
    LOG_EXT();
    return OK;
}

RC DeviceContextArray::SetScratchBufArray(PHYSADDR pPa)
{
    LOG_ENT();

    UINT32 * pData32 = (UINT32*)m_pData;
    MEM_WR32(pData32, pPa);
    pPa = pPa >> 32;
    MEM_WR32(pData32 + 1, pPa);

    Printf(Tee::PriDebug, "0x%08x%08x --> Scratchpad Buffer Array Address", MEM_RD32(pData32+1), MEM_RD32(pData32));
    LOG_EXT();
    return OK;
}

RC DeviceContextArray::PrintInfo(Tee::Priority Pri, UINT08 SlotId/* = 0*/)
{
    RC rc = OK;
    UINT32 * pData32 = (UINT32 *) m_pData;

    if ( SlotId )
    {   // print Device Context
        Printf(Pri, "Device Context of Slot %d \n", SlotId);
        DeviceContext * pContext;
        CHECK_RC(GetEntry(SlotId, &pContext));
        pContext->PrintInfo(Pri, Tee::PriLow);
    }
    else
    {   // print Device Context Base Address Array
        Printf(Pri, "Device Context Base Address Array:\n");
        Printf(Pri, " Scratchpad Buffer Array Base Address");
        Printf(Pri, "  0x%08x%08x\n", MEM_RD32(pData32+1), MEM_RD32(pData32));
        pData32 += 2;

        bool isEmpty = true;
        for ( UINT32 i = 1; i < XHCI_MAX_SIZE_DEV_CONTEXT_ARRAY; i++ )
        {
            if ( m_DevCntxtArray[i] )
            {
                Printf(Pri, " Device Context %d", i);
                Printf(Pri, "  0x%08x%08x\n", MEM_RD32(pData32+1), MEM_RD32(pData32));
                isEmpty = false;
            }
            pData32 += 2;
        }

        if ( isEmpty )
        {
            Printf(Pri, " No Device Context built yet\n");
        }
    }
    return rc;
}

// Stream Context Array, shouldn't be reffered directly. Use StreamContextArrayWrapper instead
//==============================================================================
StreamContextArray::StreamContextArray()
{
    LOG_ENT();
    m_NumEntry  = 0;
    m_pData     = NULL;
    LOG_EXT();
}

StreamContextArray::~StreamContextArray()
{
    LOG_ENT();
    if ( m_pData )
    {
        MemoryTracker::FreeBuffer(m_pData);
        m_pData = NULL;
    }
    LOG_EXT();
}

RC StreamContextArray::Init(UINT16 Size, UINT32 MemBoundary, UINT32 AddrBit)
{
    LOG_ENT();
    RC rc;
    if ( ( 0 != (Size & (Size-1) ) )
         || ( Size < 4 ) )
    {
        Printf(Tee::PriError, "Invalid size %d, should be power of 2 and at least 4", Size);
        return RC::BAD_PARAMETER;
    }

    UINT32 allocSize = XHCI_SIZE_STREAM_CONTEXT * Size;
    if ( 0 == MemBoundary )
    {
        CHECK_RC(MemoryTracker::AllocBufferAligned(allocSize, &m_pData, XHCI_ALIGNMENT_STREAM_ARRAY, AddrBit, Memory::UC));
    }
    else
    {   // fixed at page boundary at this points
        CHECK_RC(MemoryTracker::AllocBufferAligned(allocSize, &m_pData, Platform::GetPageSize(), AddrBit, Memory::UC));
    }

    Printf(Tee::PriLow, "<* Stream Contest Array base address 0x%llx, %d entries\n", Platform::VirtualToPhysical(m_pData), Size);
    Memory::Fill32(m_pData, 0, allocSize/4);

    m_NumEntry  = Size;
    LOG_EXT();
    return OK;
}

RC StreamContextArray::GetBaseAddr(PHYSADDR * ppArray)
{
    if ( !ppArray )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    if ( !m_pData )
    {
        Printf(Tee::PriError, "Stream Context Array not initialized yet");
        return RC::WAS_NOT_INITIALIZED;
    }

    * ppArray = Platform::VirtualToPhysical(m_pData);
    if ( (* ppArray) & (XHCI_ALIGNMENT_STREAM_ARRAY -1) )
    {
        Printf(Tee::PriError, "Bad pointer 0x%llx, must be %d byte aligned", * ppArray, XHCI_ALIGNMENT_STREAM_ARRAY);
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

UINT32 StreamContextArray::GetSize()
{
    return m_NumEntry;
}

bool StreamContextArray::EmptyCheck(UINT32 Index)
{
    UINT32 * pData32;
    UINT32 data32 = 0;
    if ( Index )
    {   // check entry
        GetEntryBase(Index, &pData32);
        data32 |= MEM_RD32(&pData32[0]);
        data32 |= MEM_RD32(&pData32[1]);
    }
    else
    {   // check all
        for ( UINT32 i = 0; i < m_NumEntry; i++ )
        {
            GetEntryBase(i, &pData32);
            data32 |= MEM_RD32(&pData32[0]);
            data32 |= MEM_RD32(&pData32[1]);
        }
    }
    return (data32==0);
}

RC StreamContextArray::PrintInfo(Tee::Priority Pri)
{
    RC rc;
    for (UINT32 i = 0; i < m_NumEntry; i++  )
    {
        UINT32 * pData32;
        CHECK_RC(GetEntryBase(i, &pData32));
        if ( MEM_RD32(&pData32[0]) | MEM_RD32(&pData32[1]) )
        {
            Printf(Pri, "  [%03d/%03d] 0x%08x 0x%08x \n", i, m_NumEntry, MEM_RD32(&pData32[0]), MEM_RD32(&pData32[1]));
        }
    }
    return OK;
}

RC StreamContextArray::GetEntryBase(UINT32 Index, UINT32 ** ppData)
{
    if ( !ppData )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    if ( Index >= m_NumEntry )
    {
        Printf(Tee::PriError, "Invalid Index %d, valid [0-%d]", Index, m_NumEntry-1);
        return RC::BAD_PARAMETER;
    }
    * ppData = (UINT32 *) ((UINT08 *) m_pData + XHCI_SIZE_STREAM_CONTEXT * Index);
    return OK;
}

RC StreamContextArray::ParseContext(StreamContext * pCnxt, bool * pIsDcs, UINT08 * pSct, PHYSADDR * ppPa)
{
    if ( !pCnxt )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    if ( pIsDcs )
    {
        *pIsDcs = (RF_VAL(XHCI_STREAM_CONTEXT_DW0_DCS, MEM_RD32(&pCnxt->DW0))==1);
    }
    if ( pSct )
    {
        *pSct = RF_VAL(XHCI_STREAM_CONTEXT_DW0_SCT, MEM_RD32(&pCnxt->DW0));
    }
    if ( ppPa )
    {
        *ppPa = RF_VAL(XHCI_STREAM_CONTEXT_DW1_TR_DEQ_HI, MEM_RD32(&pCnxt->DW1));
        *ppPa <<= 32;
        *ppPa |= RF_VAL(XHCI_STREAM_CONTEXT_DW0_TR_DEQ_LOW, MEM_RD32(&pCnxt->DW0)) << (0?XHCI_STREAM_CONTEXT_DW0_TR_DEQ_LOW);
    }
    return OK;
}

RC StreamContextArray::SetupContext(UINT32 Index, bool IsDcs, UINT08 Sct, PHYSADDR pPa)
{
    RC rc;
    if ( pPa & ( (0x1 << (0?XHCI_STREAM_CONTEXT_DW0_TR_DEQ_LOW)) -1 ) )
    {
        Printf(Tee::PriError, "Bad alignment, should be %d Byte aligned", 0x1 << (0?XHCI_STREAM_CONTEXT_DW0_TR_DEQ_LOW));
        return RC::BAD_PARAMETER;
    }
    UINT32 * pEntry;
    CHECK_RC(GetEntryBase(Index, &pEntry));

    MEM_WR32(&pEntry[0], pPa);
    UINT32 data32 = pPa >> 32;
    MEM_WR32(&pEntry[1], FLD_SET_RF_NUM(XHCI_STREAM_CONTEXT_DW1_TR_DEQ_HI, data32, MEM_RD32(&pEntry[1])));

    MEM_WR32(&pEntry[0], FLD_SET_RF_NUM(XHCI_STREAM_CONTEXT_DW0_DCS, IsDcs?1:0, MEM_RD32(&pEntry[0])));
    MEM_WR32(&pEntry[0], FLD_SET_RF_NUM(XHCI_STREAM_CONTEXT_DW0_SCT, Sct, MEM_RD32(&pEntry[0])));
    MEM_WR32(&pEntry[2], 0);
    MEM_WR32(&pEntry[3], 0);
    return OK;
}

RC StreamContextArray::ClearContext(UINT32 Index)
{
    RC rc;
    UINT32 * pEntry;
    CHECK_RC( GetEntryBase(Index, &pEntry) );
    MEM_WR32(&pEntry[0], 0);
    MEM_WR32(&pEntry[1], 0);
    MEM_WR32(&pEntry[2], 0);
    MEM_WR32(&pEntry[3], 0);

    return OK;
}

RC StreamContextArray::GetContext(UINT32 Index, StreamContext * pCnxt)
{
    RC rc;
    if ( !pCnxt )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::SOFTWARE_ERROR;
    }
    UINT32 * pEntry;
    CHECK_RC(GetEntryBase(Index, &pEntry));
    pCnxt->DW0 = MEM_RD32(&pEntry[0]);
    pCnxt->DW1 = MEM_RD32(&pEntry[1]);
    pCnxt->RSVD0 = MEM_RD32(&pEntry[2]);
    pCnxt->RSVD1 = MEM_RD32(&pEntry[3]);
    return OK;
}

RC StreamContextArray::GenSct(UINT08 *pSct, bool IsSecondary, bool IsRingPtr, UINT32 ArraySize)
{
    if ( !pSct )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::SOFTWARE_ERROR;
    }
    if ( IsSecondary )
    {
        *pSct = 0;
        return OK;
    }
    if ( IsRingPtr )
    {
        *pSct = 1;
        return OK;
    }
    if ( ArraySize & (ArraySize-1) )
    {
        Printf(Tee::PriError, "Invalid Size %d, must be power of two.", ArraySize);
        return RC::BAD_PARAMETER;
    }
    if ( ArraySize < 8 || ArraySize > 256 )
    {
        Printf(Tee::PriError, "Invalid Size %d, valid[8 - 256]", ArraySize);
        return RC::BAD_PARAMETER;
    }
    UINT32 oneMask = ArraySize -1;
    UINT32 oneBitCnt = 0;
    while( oneMask )
    {
        oneMask &= oneMask -1;
        oneBitCnt++;
    }
    *pSct = oneBitCnt -1;
    Printf(Tee::PriLow, "  Sct = %d\n", *pSct);
    return OK;
}

StreamContextArrayWrapper::StreamContextArrayWrapper(UINT08 SlotId, UINT08 Endpoint, bool IsDataOut, bool IsLinear)
{
    LOG_ENT();
    m_IsInit    = false;

    m_SlotId    = SlotId;
    m_EndPoint  = Endpoint;
    m_IsDataOut = IsDataOut;
    m_IsLinear  = IsLinear;
    m_AddrBit   = 32;

    m_pPrimaryArray = NULL;
    for ( UINT32 i = 0; i < XHCI_MAX_SIZE_STREAM_CONTEXT_ARRAY; i++ )
    {
        m_pSecondaryArray[i] = NULL;
    }
    LOG_EXT();
}

StreamContextArrayWrapper::~StreamContextArrayWrapper()
{
    LOG_ENT();

    for ( UINT32 i = 0; i < XHCI_MAX_SIZE_STREAM_CONTEXT_ARRAY; i++ )
    {
        if (m_pSecondaryArray[i])
        {
            delete m_pSecondaryArray[i];
        }
    }
    if (m_pPrimaryArray)
    {
        delete m_pPrimaryArray;
    }

    LOG_EXT();
}

RC StreamContextArrayWrapper::Init(UINT32 Size, UINT32 AddrBit)
{
    RC rc;
    LOG_ENT();

    if ( ( m_IsLinear && Size > 65536 )
         || ( !m_IsLinear && Size > XHCI_MAX_SIZE_STREAM_CONTEXT_ARRAY ) )
    {
        Printf(Tee::PriError, "Context array size %d overflow", Size);
        return RC::BAD_PARAMETER;
    }

    m_pPrimaryArray = new StreamContextArray();
    CHECK_RC(m_pPrimaryArray->Init(Size, m_IsLinear?0:Platform::GetPageSize(), AddrBit));
    m_AddrBit = AddrBit;

    m_IsInit = true;
    LOG_EXT();
    return rc;
}

RC StreamContextArrayWrapper::SetupEntry(UINT32 Index, UINT32 Size)
{   // create Secondary Array with given Size and insert it into entry Index of Primary Array
    // update the SCT and TD Deq Ptr of relative Primary Array entry accordingly
    RC rc;
    LOG_ENT();

    if ( m_IsLinear )
    {
        Printf(Tee::PriError, "Primary Stream Conetext Array is in Linear Mode. No Secondary Array allowed");
        return RC::BAD_PARAMETER;
    }
    if ( Size > XHCI_MAX_SIZE_STREAM_CONTEXT_ARRAY )
    {
        Printf(Tee::PriError, "Context array size %d overflow", Size);
        return RC::BAD_PARAMETER;
    }
    if ( Index >= m_pPrimaryArray->GetSize() )
    {
        Printf(Tee::PriError, "Index %d overflow, valid[0-%d]", Size, m_pPrimaryArray->GetSize()-1);
        return RC::BAD_PARAMETER;
    }
    if ( m_pSecondaryArray[Index] )
    {
        CHECK_RC(DelEntry(Index));
        if ( 0 == Size )
        {
            CHECK_RC(m_pPrimaryArray->ClearContext(Index));
            Printf(Tee::PriNormal, "Entry %d of Primary Array deleted", Index);
            return OK;
        }
    }
    // create Secondary Array with Size entries @ Index
    m_pSecondaryArray[Index] = new StreamContextArray();
    CHECK_RC(m_pSecondaryArray[Index]->Init(Size, m_AddrBit));

    // update Primary Array
    PHYSADDR pPa;
    CHECK_RC(m_pSecondaryArray[Index]->GetBaseAddr(&pPa));
    UINT08 sct;
    CHECK_RC(StreamContextArray::GenSct(&sct, false, false, Size));
    CHECK_RC(m_pPrimaryArray->SetupContext(Index, true, sct, pPa));     // Dcs ignored here

    LOG_EXT();
    return rc;
}

RC StreamContextArrayWrapper::DelEntry(UINT32 Index)
{   // remove Secondary Array from entry of Primary Array,
    // check if Secondary Array is empty. if not, pop errorLOG_EXT();
    LOG_ENT();
    RC rc;
    if ( NULL != m_pSecondaryArray[Index] )
    {
        delete m_pSecondaryArray[Index];
    }

    // update Primary Array
    CHECK_RC(m_pPrimaryArray->ClearContext(Index));
    LOG_EXT();
    return OK;
}

RC StreamContextArrayWrapper::Init(vector<UINT32> * pvSizes)
{
    RC rc;
    LOG_ENT();
    // all the checks exist in ilwoked functions
    CHECK_RC(Init(pvSizes->size()));
    for( UINT32 i = 0; i < pvSizes->size(); i ++ )
    {
        if ( (*pvSizes)[i] )
        {
            CHECK_RC( SetupEntry(i, (*pvSizes)[i]) );
        }
    }

    LOG_EXT();
    return rc;
}

RC StreamContextArrayWrapper::SetRing(UINT32 StreamId, PHYSADDR pXferRing, bool IsDcs)
{   // store the pointer of Transfer Ring to specified slot
    LOG_ENT();
    RC rc;
    UINT32 priIndex = 0;
    UINT32 secIndex = 0;

    CHECK_RC(ParseStreamId(StreamId, &priIndex, &secIndex));

    UINT08 sct;
    if ( 0 == secIndex )
    {
        //check if there's a sec array exists in that slot
        StreamContext streamCntxt;
        CHECK_RC(m_pPrimaryArray->GetContext(priIndex, &streamCntxt));
        UINT08 sct;
        CHECK_RC(StreamContextArray::ParseContext(&streamCntxt, NULL, &sct, NULL));
        if ( sct > 1 )
        {
            Printf(Tee::PriError, "Secondary Array exists in entry %d of Primary Array, TransferRing insert fail", priIndex);
            return RC::BAD_PARAMETER;
        }
        // pointer stores into Primary Array
        CHECK_RC(StreamContextArray::GenSct(&sct, false, true));
        CHECK_RC(m_pPrimaryArray->SetupContext(priIndex, IsDcs, sct, pXferRing));
    }
    else
    {   // pointer stores into Secondary Array
        if ( !m_pSecondaryArray[priIndex] )
        {
            Printf(Tee::PriError, "No Secondary Array exist in Entry %d of Primary Array, call SetupEntry() first", priIndex);
            return RC::BAD_PARAMETER;
        }
        CHECK_RC(StreamContextArray::GenSct(&sct, true));
        CHECK_RC(m_pSecondaryArray[priIndex]->SetupContext(secIndex, IsDcs, sct, pXferRing));
    }
    LOG_EXT();
    return OK;
}

bool StreamContextArrayWrapper::IsThatMe(UINT08 SlotId, UINT08 Endpoint, bool IsDataOut)
{
    return ((m_SlotId == SlotId) && (m_EndPoint == Endpoint) && (m_IsDataOut == IsDataOut));
}

bool StreamContextArrayWrapper::IsThatMe(UINT08 SlotId)
{
    return (m_SlotId == SlotId);
}

RC StreamContextArrayWrapper::ParseStreamId(UINT32 StreamId, UINT32 * pPsid, UINT32 * pSsid)
{
    if ( !pPsid || !pSsid )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    if ( 0 == StreamId )
    {
        Printf(Tee::PriError, "0 is a reserved Stream ID\n");
        return RC::BAD_PARAMETER;
    }
    *pPsid = 0;
    *pSsid = 0;

    if ( !m_IsLinear )
    {   // cascade array
        UINT32 pidMask = m_pPrimaryArray->GetSize() - 1;
        *pPsid = StreamId & pidMask;
        UINT32 oneBitCnt = 0;
        while( pidMask )
        {
            pidMask &= pidMask -1;
            oneBitCnt++;
        }
        *pSsid = StreamId >> oneBitCnt;
    }
    else
    {
        *pPsid = StreamId;
    }

    if ( *pPsid >= m_pPrimaryArray->GetSize()
         || 0 == *pPsid )
    {
        Printf(Tee::PriError, "Invalid Primary Index %d, valid[1-%d]", *pPsid, m_pPrimaryArray->GetSize()-1);
        return RC::BAD_PARAMETER;
    }
    if ( *pSsid )
    {
        if ( NULL == m_pSecondaryArray[*pPsid] )
        {
            Printf(Tee::PriError, "No Secondary Array exist in Entry %d of Primary Array, call SetupEntry() first", *pPsid);
            return RC::BAD_PARAMETER;
        }
        if ( *pSsid >= m_pSecondaryArray[*pPsid]->GetSize()
             || 0 == *pSsid )
        {
            Printf(Tee::PriError, "Invalid Secondary Index %d, valid[1-%d]", *pSsid, m_pSecondaryArray[*pPsid]->GetSize()-1);
            return RC::BAD_PARAMETER;
        }
    }

    Printf(Tee::PriLow, "PSID = %d, SSID = %d\n", *pPsid, *pSsid);
    return OK;
}

RC StreamContextArrayWrapper::GetBaseAddr(PHYSADDR * ppArray)
{
    if ( !m_pPrimaryArray )
    {
        Printf(Tee::PriError, "Context not initialized\n");
        return RC::SOFTWARE_ERROR;
    }
    return m_pPrimaryArray->GetBaseAddr(ppArray);
}

RC StreamContextArrayWrapper::PrintInfo(Tee::Priority Pri)
{
    RC rc;
    Printf(Pri, "Stream Context Array for Slot %d, Endpointer %d, Data %s (%d Entries) \n",
           m_SlotId, m_EndPoint, m_IsDataOut?"Out":"In", m_pPrimaryArray->GetSize());
    for (UINT32 i = 0; i < m_pPrimaryArray->GetSize(); i++)
    {   // only print entry with data
        StreamContext myCnxt;
        m_pPrimaryArray->GetContext(i, &myCnxt);
        if ( myCnxt.DW0 | myCnxt.DW1 )
        {
            Printf(Pri, " [%03d] 0x%08x 0x%08x ", i, myCnxt.DW0, myCnxt.DW0);
            UINT08 sct;
            CHECK_RC(StreamContextArray::ParseContext(&myCnxt, NULL, &sct, NULL));
            if ( sct == 1 )
            {
                Printf(Pri, " --> Xfer Ring \n");
            }
            else
            {
                Printf(Pri, " [%d Items]: \n", 1 << (sct+1));
                m_pSecondaryArray[i]->PrintInfo(Pri);
            }
        }
    }
    Printf(Pri, "<- End of Stream Context Array \n");
    return OK;
}
