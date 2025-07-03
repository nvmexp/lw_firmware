/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2016,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/tee.h"
#include "cheetah/include/logmacros.h"
#include "cheetah/include/tegradrf.h"
#include "cheetah/include/tegradrf.h"
#include "device/include/memtrk.h"
#include "random.h"
#include "core/include/platform.h"
#include "xhcicomn.h"
#include "xhcictrl.h"
#include "xhciring.h"
#include "xhcitrb.h"
#include "assert.h"

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME "XhciRing"

XhciTrbRing::XhciTrbRing()
{
    LOG_ENT();
    m_RingType      = XHCI_RING_TYPE_NA;

    m_pEnqPtr       = NULL;
    m_pDeqPtr       = NULL;

    m_CycleBit      = true;

    m_LwrrentSegment= 0;
    m_LwrrentIndex  = 0;
    m_RingAlignment = 0;

    m_PrintSegment  = 0;
    m_PrintIndex    = 0;

    m_TrbSeglist.clear();
    LOG_EXT();
}

XhciTrbRing::~XhciTrbRing()
{
    LOG_ENT();
    // release all the resource
    UINT32 numSegments = m_TrbSeglist.size();
    for ( UINT32 i = 0; i<numSegments; i++ )
    {
        RemoveTrbSeg(0, false);
    }
    LOG_EXT();
}

RC XhciTrbRing::AppendTrbSeg(UINT32 NumTrb,
                             INT32 PrevSegIndex/* in most case = -1*/,
                             bool IsHandleLink,
                             UINT32 AddrBit /* = 32*/)
{   // we don't consider the Deq and Enq status in this function. Child classes may take
    // consider of that as mentioned in section 4.9.2.3 and 4.9.4.1 of Spec
    LOG_ENT();
    if ( !NumTrb )
    {   // if NumTrb = 0, allocate max size segment
        NumTrb = GetTrbPerSegment();
    }
    Printf(Tee::PriDebug,"(RingType %u, NumTrb %d, PrevIndex %i\n)",
           m_RingType, NumTrb, PrevSegIndex);
    RC rc;

    UINT32 minTrbNum = 2;
    if ( (!IsEventRing()) && (m_TrbSeglist.size()==0) )
    {
        minTrbNum = 3;
    }
    if ( NumTrb < minTrbNum )
    {
        Printf(Tee::PriError, "TRB ring segment should has at least %u effective TRBs \n", minTrbNum);
        return RC::BAD_PARAMETER;
    }
    if ( NumTrb > GetTrbPerSegment() )
    {
        Printf(Tee::PriError, "Max number of TRB in a Ring is %d \n", GetTrbPerSegment());
        return RC::BAD_PARAMETER;
    }
    UINT32 prevSegment;
    if ( -1 != PrevSegIndex )
    {
        if ( Abs(PrevSegIndex) >= m_TrbSeglist.size() )
        {
            Printf(Tee::PriError, "Parent segment %d not exist \n", PrevSegIndex);
            return RC::BAD_PARAMETER;
        }
        prevSegment = Abs(PrevSegIndex);
    }
    else
    {
        prevSegment = m_TrbSeglist.size() - 1;
    }

    // allocate memory
    void * pVa = NULL;
    CHECK_RC(MemoryTracker::AllocBufferAligned(NumTrb * XHCI_TRB_LEN_BYTE, &pVa,
                                               GetTrbPerSegment() * XHCI_TRB_LEN_BYTE, AddrBit, Memory::UC));
    Memory::Fill32(pVa, 0, NumTrb * XHCI_TRB_LEN_BYTE/4);

    // preserve the start addresses of every segment.
    // be careful when release the memory, always release the whole page.
    MemoryFragment::FRAGMENT myFrag;
    myFrag.Address = pVa;
    myFrag.ByteSize = NumTrb * XHCI_TRB_LEN_BYTE;

    // Insert the segment info into vector
    {
        m_TrbSeglist.insert(m_TrbSeglist.begin()+ (prevSegment + 1), myFrag);
    }

    if ( 1 == m_TrbSeglist.size() )
    {   // if this is the first segment, init its Enq and Deq pointer.
        CHECK_RC(GetTrb(0, 0, &m_pEnqPtr));
        CHECK_RC(GetTrb(0, 0, &m_pDeqPtr));
    }

    // deal with the link trb
    if ( IsHandleLink )
    {
        if ( 1 == m_TrbSeglist.size() )
        {
            // Initialize the Link TRB in the newly append ring
            XhciTrb * pTrb;
            CHECK_RC(GetTrb(0, -1, &pTrb));
            PHYSADDR pPa = Platform::VirtualToPhysical(myFrag.Address);
            CHECK_RC(TrbHelper::SetupLinkTrb(pTrb, pPa, true));
        }
        else
        {   // Initialize the Link TRB in the newly append ring
            XhciTrb * pNewTrb;
            XhciTrb * pOldTrb;
            {   // duplicate the tail TRB of the given segment
                CHECK_RC(GetTrb(prevSegment, -1, &pOldTrb));
            }
            CHECK_RC(GetTrb(m_TrbSeglist.size()-1, -1, &pNewTrb));
            CHECK_RC(TrbHelper::TrbCopy(pNewTrb, pOldTrb));

            // setup link TRB of previous segment
            PHYSADDR pPa = Platform::VirtualToPhysical(myFrag.Address);
            CHECK_RC(TrbHelper::SetupLinkTrb(pOldTrb, pPa, false));
        }
    }

    // update the inner segment pointer to keep sync
    if ( m_LwrrentSegment > prevSegment )
    {
        m_LwrrentSegment++;
    }

    LOG_EXT();
    return OK;
}

UINT32 XhciTrbRing::GetTrbPerSegment()
{
    // see also Table 50: Data Structure Max Size, Boundary, and Alignment Requirement Summary
    // All Rings support up to 64K segment.
    return 65536 / XHCI_TRB_LEN_BYTE;
}

RC XhciTrbRing::RemoveTrbSeg(UINT32 SegIndex, bool IsHandleLink)
{
    LOG_ENT();
    RC rc;

    if ( SegIndex >= m_TrbSeglist.size() )
    {
        Printf(Tee::PriError, "Invalid Index %d ,total segment %d \n", SegIndex, (UINT32)m_TrbSeglist.size());
        return RC::BAD_PARAMETER;
    }

    // update the Link TRB of previous segment if exists
    if ( (m_TrbSeglist.size() > 1)
         && IsHandleLink )
    {
        UINT32 prevSegIndex = m_TrbSeglist.size() - 1;
        if ( 0 != SegIndex )
        {
            prevSegIndex = SegIndex - 1;
        }
        XhciTrb * pTrbToBeUpdate;
        CHECK_RC(GetTrb(prevSegIndex, -1, &pTrbToBeUpdate));
        XhciTrb * pTrbToBeDel;
        CHECK_RC(GetTrb(SegIndex, -1, &pTrbToBeDel));
        bool isTc;
        CHECK_RC(TrbHelper::GetTc(pTrbToBeUpdate, & isTc));
        CHECK_RC(TrbHelper::TrbCopy(pTrbToBeUpdate, pTrbToBeDel));
        if ( isTc )
        {   // we don't want to lose the TC bit in any TRB
            CHECK_RC(TrbHelper::SetTc(pTrbToBeUpdate, isTc));
        }
    }

    // de-alloc
    MemoryTracker::FreeBuffer(m_TrbSeglist[SegIndex].Address);

    // remove the item
    m_TrbSeglist.erase(m_TrbSeglist.begin() + SegIndex);

    // update the inner segment pointer to keep sync
    if ( m_LwrrentSegment > SegIndex )
    {
        m_LwrrentSegment--;
    }
    LOG_EXT();
    return OK;
}

RC XhciTrbRing::GetBaseAddr(PHYSADDR * ppPhyBase)
{
    RC rc;
    if ( m_TrbSeglist.size() == 0 )
    {
        Printf(Tee::PriError, "No ring defined \n");
        return RC::BAD_PARAMETER;
    }
    if ( !ppPhyBase )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    XhciTrb * pBase;
    CHECK_RC(GetTrb(0, 0, &pBase));

    *ppPhyBase = Platform::VirtualToPhysical((void *)pBase);
    if ( 0 == m_RingAlignment )
    {
        Printf(Tee::PriError, "Alignment requirement not set! \n");
        return RC::SOFTWARE_ERROR;
    }
    if ( (*ppPhyBase) & (m_RingAlignment - 1) )
    {
        Printf(Tee::PriError, "Bad pointer 0x%llx, must be %d byte aligned \n", * ppPhyBase, m_RingAlignment);
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

RC XhciTrbRing::GetEmptySlots(UINT32 *pNumSlots)
{
    RC rc;
    if ( (!IsXferRing() )
         && ( !IsCmdRing() ) )
    {
        Printf(Tee::PriError, "function only apply to Command and Transfer Rings \n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    UINT32 numSlots = 0;

    UINT32 deqSeg, deqIndex;
    CHECK_RC(FindTrb(m_pDeqPtr, &deqSeg, &deqIndex));

    UINT32 enqSeg = m_LwrrentSegment;
    UINT32 enqIndex = m_LwrrentIndex;
    if ( enqSeg == deqSeg )
    {
        if ( enqIndex >= deqIndex )
        {
            numSlots = m_TrbSeglist[enqSeg].ByteSize / XHCI_TRB_LEN_BYTE - 1 - enqIndex;
            enqSeg = (enqSeg + 1)%m_TrbSeglist.size();
            enqIndex = 0;
        }
        else
        {
            numSlots = deqIndex - enqIndex;
            deqIndex = 0;
        }
    }
    while ( enqSeg != deqSeg )
    {
        numSlots += m_TrbSeglist[enqSeg].ByteSize / XHCI_TRB_LEN_BYTE - 1 - enqIndex;
        enqSeg = (enqSeg + 1)%m_TrbSeglist.size();
        enqIndex = 0;
    }
    numSlots += deqIndex;
    // be careful here. the FULL definition says: If the "Enqueue Pointer + 1" = Dequeue Pointer, then the ring is full
    // means if DeqPtr - EnqPtr = 1, this function should return 0
    *pNumSlots = numSlots - 1;
    Printf(Tee::PriDebug, "  %u empty slots left", numSlots - 1);
    LOG_EXT();
    return OK;
}

RC XhciTrbRing::FindTrb(XhciTrb* pNeedle,
                        UINT32 * pSeg,
                        UINT32 * pIndex,
                        bool IsSilent)
{
    RC rc;
    if ( (!pSeg) || (!pIndex) )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    UINT32 segIndex;
    UINT32 entryIndex = 0;
    bool isFound = false;
    for ( segIndex = 0; segIndex < m_TrbSeglist.size(); segIndex++ )
    {
        XhciTrb * pStart;
        CHECK_RC(GetTrb(segIndex, 0, &pStart));
        XhciTrb * pEnd;
        CHECK_RC(GetTrb(segIndex, -1, &pEnd));
        if ( (pNeedle >= pStart) && (pNeedle <= pEnd) )
        { // found
            entryIndex = (UINT32)(pNeedle - pStart);    //pNeedle is pointer to XhciTrb
            isFound = true;
            break;
        }
    }
    if ( !isFound )
    {
        if ( !IsSilent )
        {
            Printf(Tee::PriError, "Given TRB not found in the Ring \n");
        }
        return RC::SOFTWARE_ERROR;
    }
    else if ( (m_TrbSeglist[segIndex].ByteSize / XHCI_TRB_LEN_BYTE) < (2 + entryIndex) )
    {
        // Printf(Tee::PriError, "Strange: given TRB is at the position of a Link TRB");
        // return RC::SOFTWARE_ERROR;
    }
    *pSeg = segIndex;
    *pIndex = entryIndex;
    return OK;
}

RC XhciTrbRing::FindGap(XhciTrb* pTrbStart,
                        XhciTrb* pTrbEnd,
                        UINT32 * pNumSlots)
{   // count from Start forward to End
    RC rc;
    if ( !pNumSlots )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }

    UINT32 segStart, indexStart;
    CHECK_RC(FindTrb(pTrbStart, &segStart, &indexStart));
    UINT32 segEnd, indexEnd;
    CHECK_RC(FindTrb(pTrbEnd, &segEnd, &indexEnd));

    UINT32 numSlot = 0;
    UINT32 slotToNextSeg = m_TrbSeglist[segStart].ByteSize / XHCI_TRB_LEN_BYTE - indexStart;
    while ( segStart != segEnd )
    {
        numSlot += slotToNextSeg;
        segStart++;
        if ( segStart >= m_TrbSeglist.size() )
            segStart = 0;
        indexStart = 0;
        slotToNextSeg = m_TrbSeglist[segStart].ByteSize / XHCI_TRB_LEN_BYTE;
    }
    numSlot += indexEnd - indexStart;
    Printf(Tee::PriLow, "  FindGap: %u slots\n", numSlot);

    * pNumSlots = numSlot;
    return OK;
}

// Get forward TRB from given position
RC XhciTrbRing::GetForwardEntry(UINT32 FowardLevel,
                                XhciTrb * pNeedle,
                                XhciTrb ** ppTrb,
                                UINT32 *pSeg,
                                UINT32 *pIndex)
{
    RC rc;
    if ( !IsXferRing()
         && !IsCmdRing() )
    {
        Printf(Tee::PriError, "function only apply to Command and Transfer Rings \n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    CHECK_RC(VerifyPointer());
    // find the corresponding SegIndex and Entry Index
    UINT32 segIndex;
    UINT32 entryIndex;
    CHECK_RC(FindTrb(pNeedle, &segIndex, &entryIndex));
    // e.g. a segment with n entries could allow forwarding n-1 to get to the next segment
    UINT32 itemToNextSeg = m_TrbSeglist[segIndex].ByteSize / XHCI_TRB_LEN_BYTE - entryIndex - 1;
    while ( FowardLevel >= itemToNextSeg )
    {
        FowardLevel -= itemToNextSeg;
        segIndex = (segIndex + 1) % m_TrbSeglist.size();
        entryIndex = 0;
        itemToNextSeg = m_TrbSeglist[segIndex].ByteSize / XHCI_TRB_LEN_BYTE - entryIndex - 1;
    }

    if ( pSeg && pIndex )
    {
        *pSeg = segIndex;
        *pIndex = FowardLevel + entryIndex;
        return OK;
    }
    return GetTrb(segIndex, FowardLevel + entryIndex, ppTrb);
}

RC XhciTrbRing::GetBackwardEntry(UINT32 BackLevel,
                                 XhciTrb ** ppTrb,
                                 UINT32 *pSeg,
                                 UINT32 *pIndex)
{
    RC rc;
    CHECK_RC(VerifyPointer());

    UINT32 segIndex = m_LwrrentSegment;
    UINT32 itemLeftInSeg = m_LwrrentIndex;
    while ( itemLeftInSeg < BackLevel )
    {
        BackLevel -= itemLeftInSeg;
        if ( segIndex > 0 )
            segIndex--;
        else
            segIndex = m_TrbSeglist.size() - 1;
        itemLeftInSeg = m_TrbSeglist[segIndex].ByteSize / XHCI_TRB_LEN_BYTE - (IsEventRing()?0:1);    // -1 to remove the Link TRB
    }

    if (pSeg && pIndex)
    {
        *pSeg = segIndex;
        *pIndex = itemLeftInSeg - BackLevel;
    }
    return GetTrb(segIndex, itemLeftInSeg - BackLevel, ppTrb);
}

RC XhciTrbRing::GetEnqInfo(PHYSADDR * ppPa, bool * IsPcs, XhciTrb **ppEnqPtr)
{
    RC rc;
    if ( !IsXferRing()
         && !IsCmdRing() )
    {
        Printf(Tee::PriError, "function only apply to Command and Transfer Rings \n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    CHECK_RC(VerifyPointer());

    *ppPa = Platform::VirtualToPhysical(m_pEnqPtr);
    *IsPcs = m_CycleBit;
    if ( ppEnqPtr )
    {
        *ppEnqPtr = m_pEnqPtr;
    }
    return OK;
}

// this is a debug protection funtion, might be removed after MODS get stable
RC XhciTrbRing::VerifyPointer()
{
    if ( m_LwrrentSegment >= m_TrbSeglist.size() )
    {
        Printf(Tee::PriError, "Segment %d over flow \n", m_LwrrentSegment);
        return RC::SOFTWARE_ERROR;
    }
    if ( ( m_LwrrentIndex * XHCI_TRB_LEN_BYTE ) >= m_TrbSeglist[m_LwrrentSegment].ByteSize )
    {
        Printf(Tee::PriError, "Index %d over flow \n", m_LwrrentIndex);
        return RC::SOFTWARE_ERROR;
    }
    void * pTmp = (UINT08 *)m_TrbSeglist[m_LwrrentSegment].Address + m_LwrrentIndex * XHCI_TRB_LEN_BYTE;
    if ( IsXferRing()
         || IsCmdRing() )
    {
        if ( pTmp != m_pEnqPtr )
        {
            Printf(Tee::PriError, "Pointer mismatch, LwrrentSeg %u, LwrrentIndex %u, EnqPtr %p, expected %p \n", m_LwrrentSegment, m_LwrrentIndex, m_pEnqPtr, pTmp);
            return RC::SOFTWARE_ERROR;
        }
    }
    else
    {   // Event Ring
        if ( pTmp != m_pDeqPtr )
        {
            Printf(Tee::PriError, "Event Ring pointer mismatch, LwrrentSeg %u, LwrrentIndex %u, DeqPtr %p, expected %p \n", m_LwrrentSegment, m_LwrrentIndex, m_pDeqPtr, pTmp);
            return RC::SOFTWARE_ERROR;
        }
    }
    return OK;
}

// do basic initialize to a new TRB slot. init the Cycle Bit & write 0 to rest bits
RC XhciTrbRing::InsertEmptyTrb(XhciTrb ** ppTrb)
{
    LOG_ENT();
    RC rc;

    bool isFull;
    CHECK_RC(IsFull(&isFull));
    if ( isFull )
    {
        Printf(Tee::PriError, "Ring is full, try append more segment or wait for empty slot \n");
        return RC::OUT_OF_HW_RSC;
    }
    if ( !ppTrb )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::SOFTWARE_ERROR;
    }
    if ( !IsXferRing()
         && !IsCmdRing() )
    {
        Printf(Tee::PriError, "function only apply to Command and Transfer Rings \n");
        return RC::BAD_PARAMETER;
    }

    CHECK_RC(VerifyPointer());

    // blank out the TRB without touch the Cycle bit
    CHECK_RC(TrbHelper::ClearTrb(m_pEnqPtr));
    // set the return value
    *ppTrb = m_pEnqPtr;

    // advance the Enq and Segment/Index pair
    // get the segment and position of next TRB slot available
    UINT32 numItem = m_TrbSeglist[m_LwrrentSegment].ByteSize / XHCI_TRB_LEN_BYTE;
    if ( (numItem - m_LwrrentIndex) > 2 )
    {   // there's enough TRB slot left
        m_LwrrentIndex++;
        m_pEnqPtr++;
    }
    else
    {   // not enough TRB slots left, move to the next segment
        // if the link trb we across has the TC bit set, toggle the m_CycleBit accordingly
        // before move to the next segment, set the Cycle bit in the Link TRB accrossing
        XhciTrb * pTrb;
        CHECK_RC(GetTrb(m_LwrrentSegment, -1, &pTrb));
        CHECK_RC(TrbHelper::SetCycleBit(pTrb, m_CycleBit)); //enable this Link TRB

        bool isTc;
        CHECK_RC(TrbHelper::GetTc(pTrb, &isTc));
        if ( isTc )
        {
            Printf(Tee::PriDebug, "TC toggled\n");
            m_CycleBit = !m_CycleBit;
        }

        // Advance the pointer to the next empty TRB
        m_LwrrentSegment = (m_LwrrentSegment+1) % m_TrbSeglist.size();
        m_LwrrentIndex = 0;
        CHECK_RC(GetTrb(m_LwrrentSegment, m_LwrrentIndex, &m_pEnqPtr));
    }
    Printf(Tee::PriDebug, "  TRB @ 0x%llx(virt:%p) now avaliable", Platform::VirtualToPhysical(*ppTrb), *ppTrb);
    LOG_EXT();
    return OK;
}

RC XhciTrbRing::UpdateDeqPtr(XhciTrb* pFinishedTrb, bool IsAdvancePtr)
{
    RC rc;
    if ( !IsXferRing()
         && !IsCmdRing() )
    {
        Printf(Tee::PriError, "function only apply to Command and Transfer Rings \n");
        return RC::BAD_PARAMETER;
    }

    XhciTrb * pDeqPtr;
    CHECK_RC(GetForwardEntry(IsAdvancePtr?1:0, pFinishedTrb, &pDeqPtr));

    m_pDeqPtr = pDeqPtr;
    Printf(Tee::PriDebug, "%p(virt)-->%s Ring Deq Pointer\n", pDeqPtr, IsXferRing()?"Xfer":"Cmd");
    return OK;
}

RC XhciTrbRing::IsOwnBySw(XhciTrb *pTrb, bool *pIsBySw)
{
    RC rc;
    if ( !IsXferRing()
         && !IsCmdRing() )
    {
        Printf(Tee::PriError, "function only apply to Command and Transfer Rings \n");
        return RC::BAD_PARAMETER;
    }

    if ( (!pTrb) || (!pIsBySw) )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    *pIsBySw = false;
    UINT32 slotGap1, slotGap2;
    // case1 by HW: |||||Deq---------Enq||||TRB|||
    // case2 by SW: |||||Deq---------TRB----Enq|||
    // gap2 = Deq------>Enq
    // gap1 = Deq------>TRB
    // if gap2<=gap1, owned by SW
    CHECK_RC(FindGap(m_pDeqPtr, pTrb, &slotGap1));
    CHECK_RC(FindGap(m_pDeqPtr, m_pEnqPtr, &slotGap2));
    if ( slotGap2 <= slotGap1 )
        *pIsBySw = true;

    return OK;
}

RC XhciTrbRing::IsEmpty(bool * pIsEmpty)
{   // is Enq = Deq, it's empty
    if ( !IsXferRing()
         && !IsCmdRing() )
    {
        Printf(Tee::PriError, "function only apply to Command and Transfer Rings \n");
        return RC::BAD_PARAMETER;
    }
    if ( !pIsEmpty )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::SOFTWARE_ERROR;
    }
    *pIsEmpty = (m_pDeqPtr == m_pEnqPtr);
    return OK;
}

RC XhciTrbRing::IsFull(bool * pIsFull)
{   // if advancing the Enq would make it equal to Deq, then Ring is full. take consider of Link Trb
    RC rc;

    if ( !IsXferRing()
         && !IsCmdRing() )
    {
        Printf(Tee::PriError, "function only apply to Command and Transfer Rings \n");
        return RC::BAD_PARAMETER;
    }
    if ( !pIsFull )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::SOFTWARE_ERROR;
    }

    XhciTrb * pNextTrb = NULL;
    UINT32 numItemOfSeg = m_TrbSeglist[m_LwrrentSegment].ByteSize / XHCI_TRB_LEN_BYTE;

    if ( m_LwrrentIndex >= numItemOfSeg - 2 )
    {   // across the Link TRB to the next base of Segment
        CHECK_RC(GetTrb((m_LwrrentSegment+1)%m_TrbSeglist.size(), 0, &pNextTrb));
    }
    else
    {
        pNextTrb = m_pEnqPtr + 1;
    }

    * pIsFull = false;
    if ( pNextTrb == m_pDeqPtr )
    {
        * pIsFull = true;
    }

    Printf(Tee::PriDebug, "Advanced Enq Pointer :%p, Deq Pointer :%p", pNextTrb, m_pDeqPtr);
    LOG_EXT();
    return OK;
}

bool XhciTrbRing::IsXferRing()
{
    return m_RingType == XHCI_RING_TYPE_XFER;
}

bool XhciTrbRing::IsCmdRing()
{
    return m_RingType == XHCI_RING_TYPE_CMD;
}

bool XhciTrbRing::IsEventRing()
{
    return m_RingType == XHCI_RING_TYPE_EVENT;
}

RC XhciTrbRing::GetTrbEntry(UINT32 SegIndex, UINT32 TrbIndex, XhciTrb * pTrb)
{
    RC rc;
    if ( !pTrb )
    {
        Printf(Tee::PriError, "Null Pointer\n"); return RC::BAD_PARAMETER;
    }

    XhciTrb * pTmpTrb;
    CHECK_RC(GetTrb(SegIndex, TrbIndex, &pTmpTrb));

    CHECK_RC(TrbHelper::TrbCopy(pTrb, pTmpTrb, XHCI_TRB_COPY_DEST_IN_STACK));
    return OK;
}

RC XhciTrbRing::SetTrbEntry(UINT32 SegIndex, UINT32 TrbIndex, XhciTrb * pTrb)
{
    RC rc;
    if ( !pTrb )
    {
        Printf(Tee::PriError, "Null Pointer\n"); return RC::BAD_PARAMETER;
    }

    XhciTrb * pTmpTrb;
    CHECK_RC(GetTrb(SegIndex, TrbIndex, &pTmpTrb));

    CHECK_RC(TrbHelper::TrbCopy(pTmpTrb, pTrb, XHCI_TRB_COPY_SRC_IN_STACK));
    return OK;
}

RC XhciTrbRing::SetPtr(UINT32 SegIndex, UINT32 TrbIndex, bool IsEnqPtr)
{
    RC rc;
    XhciTrb *pTrb;

    // protection is in GetTrb()
    CHECK_RC(GetTrb(SegIndex, TrbIndex, &pTrb));
    m_LwrrentSegment = SegIndex;
    m_LwrrentIndex = TrbIndex;

    if ( IsEnqPtr )
    {
        m_pEnqPtr = pTrb;
    }
    else
    {
        m_pDeqPtr = pTrb;
    }
    return OK;
}

RC XhciTrbRing::GetPtr(UINT32 * pSegment, UINT32 * pIndex)
{
    if ( !pSegment || !pIndex )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    *pSegment   = m_LwrrentSegment;
    *pIndex     = m_LwrrentIndex;
    return OK;
}

RC XhciTrbRing::GetSegmentSizes(vector<UINT32> *pvSizes)
{
    if ( !pvSizes )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    pvSizes->clear();
    for ( UINT32 i = 0; i<m_TrbSeglist.size(); i++ )
    {
        pvSizes->push_back(m_TrbSeglist[i].ByteSize / XHCI_TRB_LEN_BYTE);
    }

    return OK;
}

RC XhciTrbRing::PrintInfo(Tee::Priority Pri)
{
    RC rc;
    bool isEllipsisPrinted = false;
    Printf(Pri,
           " Deq Pointer 0x%llx, Enq Point 0x%llx, Cycle Bit %s \n",
           Platform::VirtualToPhysical(m_pDeqPtr),
           IsEventRing()?0:Platform::VirtualToPhysical(m_pEnqPtr),
           m_CycleBit?"1":"0");

    for ( UINT32 i = 0; i <m_TrbSeglist.size(); i++ )
    {
        XhciTrb* pHead;
        CHECK_RC(GetTrb(i, 0, &pHead));
        Printf(Pri,"---------------------------%d---0x%llx(virt:%p)\n", i, Platform::VirtualToPhysical(pHead), pHead);
        for ( UINT32 j = 0; j<(m_TrbSeglist[i].ByteSize / XHCI_TRB_LEN_BYTE); j++ )
        {
            XhciTrb * pTrb;
            CHECK_RC(GetTrb(i, j, &pTrb));
            if ( isEllipsisPrinted )
            {
                if ( !TrbHelper::IsTrbEmpty(pTrb)
                     || m_pDeqPtr == pTrb
                     || m_pEnqPtr == pTrb )
                {
                    // if there's an non-empty TRB emerge, DeqPtr or EnqPtr
                    // reset the ellipsis setting
                    isEllipsisPrinted = false;
                }
                else
                {   // if ellipsis has been printed, skip printing
                    continue;
                }
            }

            UINT08 trbType;
            CHECK_RC(TrbHelper::GetTrbType(pTrb, &trbType));
            // print the index number
            Printf(Pri, "[%03d] ", j);

            if ( m_pDeqPtr == pTrb )
                Printf(Pri,">");            // this is the Deq pointer
            else
                Printf(Pri," ");

            if ( trbType == XhciTrbTypeReserved )
            {
                Printf(Pri, "--------");
            }
            else
            {
                if ( !(XHCI_DEBUG_MODE_PRINT_MORE & XhciComnDev::GetDebugMode()) )
                {
                    Printf(Pri, "(0x%llx) ", Platform::VirtualToPhysical(pTrb));
                }
                TrbHelper::GetTrbType(pTrb, NULL, Pri);
            }

            bool isCBit;
            CHECK_RC(TrbHelper::GetCycleBit(pTrb, &isCBit));
            if ( isCBit )
                Printf(Pri,"*");
            else
                Printf(Pri,"~");

            if ( trbType == XhciTrbTypeLink )
            {
                bool isTc;
                CHECK_RC(TrbHelper::GetTc(pTrb, &isTc));
                if ( isTc )
                    Printf(Pri,"!");
            }

            if ( ( m_pEnqPtr == pTrb ) && (!IsEventRing()) )
                Printf(Pri,"<-- ");       // this is the Enq pointer

            if ( trbType == XhciTrbTypeLink )
            {
                PHYSADDR pLink;
                CHECK_RC(TrbHelper::GetLinkTarget(pTrb, &pLink));
                void* pTmp = MemoryTracker::PhysicalToVirtual(pLink);
                Printf(Pri," -->%p(virt)", pTmp);
            }

            if ( !(XHCI_DEBUG_MODE_PRINT_MORE & XhciComnDev::GetDebugMode()) )
            {   // dump raw TRB data
                // todo: change XHCI_DEBUG_MODE_PRINT_MORE -> XHCI_DEBUG_MODE_PRINT_LESS
                Printf(Pri,"\n  ");
                CHECK_RC(TrbHelper::DumpTrb(pTrb, Pri, false));
            }

            Printf(Pri,"\n");

            if ( (!isEllipsisPrinted)
                 && TrbHelper::IsTrbEmpty(pTrb) )
            {   // when we find a empty TRB, print an ellipsis after it
                Printf(Pri,"...... \n");
                isEllipsisPrinted = true;
            }
        }   // move to next TRB
    }
    Printf(Pri,"------------------------------\n");
    return OK;
}

RC XhciTrbRing::Clear()
{
    RC rc;

    if ( IsXferRing()
         || IsCmdRing() )
    {
        bool isRingEmpry;
        CHECK_RC(IsEmpty(&isRingEmpry));
        if ( !isRingEmpry )
        {
            Printf(Tee::PriError, "Ring is not empty, can't clear it \n");
            return RC::HW_STATUS_ERROR;
        }
    }

    for ( UINT32 i = 0; i <m_TrbSeglist.size(); i++ )
    {
        for ( UINT32 j = 0; j<(m_TrbSeglist[i].ByteSize / XHCI_TRB_LEN_BYTE); j++ )
        {
            XhciTrb * pTrb;
            CHECK_RC(GetTrb(i, j, &pTrb));
            UINT08 trbType;
            CHECK_RC(TrbHelper::GetTrbType(pTrb, &trbType));
            if ( trbType == XhciTrbTypeLink )
            {   // don't touch link TRBs
                continue;
            }
            // clear all bits to 0 but keep Cycle Bit un-touched
            CHECK_RC(TrbHelper::ClearTrb(pTrb));
        }
    }

    return OK;
}

RC XhciTrbRing::GetTrb(UINT32 Seg, INT32 Index, XhciTrb ** ppTrb)
{
    LOG_ENT();
    if ( !ppTrb )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    *ppTrb = NULL;

    if ( Seg >= m_TrbSeglist.size() )
    {
        PrintTitle(Tee::PriHigh);
        Printf(Tee::PriError, "Segment index %d overflow, valid [0-%d] \n", Seg, (UINT32)m_TrbSeglist.size()-1);
        return RC::BAD_PARAMETER;
    }

    if ( Index >=0 )
    {
        if ( Abs(Index) >= ( m_TrbSeglist[Seg].ByteSize / XHCI_TRB_LEN_BYTE) )
        {
            Printf(Tee::PriError, "Index %d overflow, total %d \n", Index,  m_TrbSeglist[Seg].ByteSize / XHCI_TRB_LEN_BYTE);
            return RC::BAD_PARAMETER;
        }
        *ppTrb = ((XhciTrb*)m_TrbSeglist[Seg].Address) + Index;
    }
    else
    { // Index < 0
        if ( Abs(Index) > ( m_TrbSeglist[Seg].ByteSize / XHCI_TRB_LEN_BYTE) )
        {
            Printf(Tee::PriError, "Index %d overflow, total %d \n", Index, m_TrbSeglist[Seg].ByteSize / XHCI_TRB_LEN_BYTE);
            return RC::BAD_PARAMETER;
        }
        *ppTrb = ((XhciTrb*)m_TrbSeglist[Seg].Address) + m_TrbSeglist[Seg].ByteSize / XHCI_TRB_LEN_BYTE + Index;
    }

    Printf(Tee::PriDebug, "TRB %p returned", *ppTrb);
    LOG_EXT();
    return OK;
}

// Command Ring
//==============================================================================
CmdRing::CmdRing(XhciController * pHost)
{
    LOG_ENT();
    m_pHost = pHost;
    m_RingType = XHCI_RING_TYPE_CMD;
    m_RingAlignment = XHCI_ALIGNMENT_CMD_RING;
    LOG_EXT();
}

CmdRing::~CmdRing()
{
    LOG_ENT();
    LOG_EXT();
}

RC CmdRing::Init(UINT32 NumElem, UINT32 AddrBit)
{
    RC rc;
    if ( !m_pHost )
    {
        Printf(Tee::PriError, "pHost NULL pointer \n");
        return RC::BAD_PARAMETER;
    }
    CHECK_RC(AppendTrbSeg(NumElem, -1, AddrBit));
    return OK;
}

RC CmdRing::SetEnqPtr(UINT32 SegIndex, UINT32 TrbIndex, INT32 CycleBit)
{
    if ( 0 != CycleBit )
    {
        m_CycleBit =  CycleBit > 0;
    }
    return XhciTrbRing::SetPtr(SegIndex, TrbIndex, true);
}

RC CmdRing::GetPcs(bool * pIsCBit)
{
    if ( !pIsCBit )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::SOFTWARE_ERROR;
    }
    * pIsCBit = m_CycleBit;
    return OK;
}

RC CmdRing::PrintInfo(Tee::Priority Pri)
{
    PrintTitle(Pri);
    return XhciTrbRing::PrintInfo(Pri);
}

RC CmdRing::PrintTitle(Tee::Priority Pri)
{
    Printf(Pri, "Command Ring \n");
    return OK;
}

RC CmdRing::AppendTrbSeg(UINT32 NumElem, INT32 PrevSegIndex, UINT32 AddrBit)
{
    LOG_ENT();
    RC rc;

    if ( m_TrbSeglist.size()>0 )
    {   // check to see if the Link TRB of parent segment is owned by HW
        if ( PrevSegIndex == -1 )
        {
            PrevSegIndex = m_TrbSeglist.size() - 1;
        }
        XhciTrb *pTrb;
        CHECK_RC(GetTrb(PrevSegIndex, -1, &pTrb));
        bool isOwnBySw;
        CHECK_RC(IsOwnBySw(pTrb, &isOwnBySw));
        if ( !isOwnBySw )
        {
            Printf(Tee::PriError, "Link TRB to be updated is owned by HW now, try it later \n");
            return RC::UNSUPPORTED_FUNCTION;
        }
    }

    // Insert the new segment at the given point
    CHECK_RC(XhciTrbRing::AppendTrbSeg(NumElem, PrevSegIndex, true, AddrBit));

    // update the Cycle bits in newly added segment to be in line with its parent
    if ( m_TrbSeglist.size() > 1 )
    {
        XhciTrb *pLastTrb;
        UINT32 parentIndex;
        if ( -1 == PrevSegIndex )
            parentIndex = m_TrbSeglist.size()-2;
        else
            parentIndex = PrevSegIndex;

        CHECK_RC(GetTrb(parentIndex, -2, &pLastTrb));
        bool cBit;
        CHECK_RC(TrbHelper::GetCycleBit(pLastTrb, &cBit));
        if ( cBit )
        {   // we need set all Cycle bits to 1 in the new segment
            for ( UINT32 i = 0; i<m_TrbSeglist[parentIndex+1].ByteSize / XHCI_TRB_LEN_BYTE; i++ )
            {
                CHECK_RC(GetTrb(parentIndex+1, i, &pLastTrb));
                CHECK_RC(TrbHelper::SetCycleBit(pLastTrb, true));
            }
        }
    }
    LOG_EXT();
    return OK;
}

RC CmdRing::RemoveTrbSeg(UINT32 SegIndex)
{
    RC rc;

    if ( m_TrbSeglist.size()>1 )
    {   // check to see if the Link TRB of previous segment is owned by HW
        UINT32 prevSegIndex;
        if ( SegIndex == 0 )
            prevSegIndex = m_TrbSeglist.size() - 1;
        else
            prevSegIndex = SegIndex - 1;

        XhciTrb *pTrb;
        CHECK_RC(GetTrb(prevSegIndex, -1, &pTrb));
        bool isOwnBySw;
        CHECK_RC(IsOwnBySw(pTrb, &isOwnBySw));
        if ( !isOwnBySw )
        {
            Printf(Tee::PriError, "Link TRB to be updated is owned by HW now, try it later \n");
            return RC::UNSUPPORTED_FUNCTION;
        }
    }
    if ( m_TrbSeglist.size() == 1 )
    {
        Printf(Tee::PriLow, "  Removing the only segment of CMD Ring\n");
    }
    else if ( m_LwrrentSegment == SegIndex )
    {
        Printf(Tee::PriError, "Enq Pointer is in Segment %u, it can not be removed \n", m_LwrrentSegment);
        return RC::BAD_PARAMETER;
    }
    return XhciTrbRing::RemoveTrbSeg(SegIndex, true);
}

RC CmdRing::IssueNoOp(XhciTrb ** ppEeqPointer,
                      bool IsIssue/* = true*/,
                      UINT32 WaitMs /*= 2000*/)
{// System software shall schedule no more than one outstanding command per Device Slot or VF.
    LOG_ENT();
    RC rc;

    if ( ppEeqPointer )
    {
        *ppEeqPointer = m_pEnqPtr;
    }
    // setup a No OP command in the TRB referred by Enq pointer
    XhciTrb * pTrb;
    CHECK_RC(InsertEmptyTrb(&pTrb));
    CHECK_RC(TrbHelper::SetupCmdNoOp(pTrb));

    // issue a device command
    if ( IsIssue )
        CHECK_RC(m_pHost->RingDoorBell());

    // wait for the commnad completion event
    if ( WaitMs != 0 )
        CHECK_RC(m_pHost->WaitCmdCompletion());

    LOG_EXT();
    return OK;
}

RC CmdRing::IssueEnableSlot(UINT08 * pSlotId, UINT08 SlotType, bool IsIssue, UINT32 WaitMs)
{
    LOG_ENT();
    RC rc;
    if ( !pSlotId )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::SOFTWARE_ERROR;
    }

    // setup a Enable Slot command in the TRB referred by Enq pointer
    XhciTrb * pTrb;
    CHECK_RC(InsertEmptyTrb(&pTrb));
    CHECK_RC(TrbHelper::SetupCmdEnableSlot(pTrb, SlotType));

    //PrintInfo(Tee::PriNormal);  //for debug!!!
    // issue a device command
    if ( IsIssue )
        CHECK_RC(m_pHost->RingDoorBell());

    // wait for the commnad completion event
    if ( WaitMs != 0 )
        CHECK_RC(m_pHost->WaitCmdCompletion(NULL, pSlotId));

    LOG_EXT();
    return OK;
}

RC CmdRing::IssueDisableSlot(UINT08 SlotId, bool IsIssue, UINT32 WaitMs)
{
    LOG_ENT();
    RC rc;
    XhciTrb * pTrb;
    CHECK_RC(InsertEmptyTrb(&pTrb));
    CHECK_RC(TrbHelper::SetupCmdDisableSlot(pTrb, SlotId));

    if ( IsIssue )
        CHECK_RC(m_pHost->RingDoorBell());

    // wait for the commnad completion event
    if ( WaitMs != 0 )
        CHECK_RC(m_pHost->WaitCmdCompletion());

    LOG_EXT();
    return OK;
}

// returned Address value will be in Slot Context
RC CmdRing::IssueAddressDev(UINT08 SlotId,
                            InputContext * pInpCntxt,
                            bool IsIssue,
                            UINT32 WaitMs,
                            bool IsBsr)
{
    LOG_ENT();
    RC rc;

    if (!pInpCntxt)
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }

    XhciTrb * pTrb;
    CHECK_RC(InsertEmptyTrb(&pTrb));

    PHYSADDR pPhy;
    CHECK_RC(pInpCntxt->GetBaseAddr(&pPhy));

    CHECK_RC(TrbHelper::SetupCmdAddrDev(pTrb, pPhy, SlotId, IsBsr));

    if ( IsIssue )
        CHECK_RC(m_pHost->RingDoorBell());

    if ( WaitMs != 0 )
        CHECK_RC(m_pHost->WaitCmdCompletion());

    LOG_EXT();
    return OK;
}

RC CmdRing::IssueCfgEndp(InputContext * pInpCntxt,
                         UINT08 SlotId,
                         bool IsDeCfg,
                         bool IsIssue,
                         UINT32 WaitMs)
{
    LOG_ENT();
    RC rc;

    if (!pInpCntxt)
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }

    XhciTrb * pTrb;
    CHECK_RC(InsertEmptyTrb(&pTrb));

    PHYSADDR pPhy;
    CHECK_RC(pInpCntxt->GetBaseAddr(&pPhy));

    CHECK_RC(TrbHelper::SetupCmdCfgEndp(pTrb, pPhy, SlotId, IsDeCfg));

    if ( IsIssue )
        CHECK_RC(m_pHost->RingDoorBell());

    if ( WaitMs != 0 )// wait for the commnad completion event
        CHECK_RC(m_pHost->WaitCmdCompletion());

    LOG_EXT();
    return OK;
}

RC CmdRing::IssueEvalContext(InputContext * pInpCntxt,
                             UINT08 SlotId,
                             bool IsIssue,
                             UINT32 WaitMs)
{
    LOG_ENT();
    RC rc;

    if (!pInpCntxt)
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    Printf(Tee::PriLow, "CmdRing::IssueEvalContext(SlotId %d) - InputContext:\n", SlotId);
    pInpCntxt->PrintInfo(Tee::PriLow);

    XhciTrb * pTrb;
    CHECK_RC(InsertEmptyTrb(&pTrb));

    PHYSADDR pPhy;
    CHECK_RC(pInpCntxt->GetBaseAddr(&pPhy));
    //void * pva = MemoryTracker::PhysicalToVirtual(pPhy);
    //Printf(Tee::PriWarn, "  Raw data dump \n");
    //Memory::Print32(pva, 4 * 16 * 4 );

    CHECK_RC(TrbHelper::SetupCmdEvalContext(pTrb, pPhy, SlotId));

    if ( IsIssue )
        CHECK_RC(m_pHost->RingDoorBell());

    if ( WaitMs != 0 )// wait for the commnad completion event
        CHECK_RC(m_pHost->WaitCmdCompletion());

    LOG_EXT();
    return OK;
}

RC CmdRing::IssueResetEndp(UINT08 SlotId,
                           UINT08 EndPoint,
                           bool IsTransferStatePreserve,
                           bool IsIssue,
                           UINT32 WaitMs)
{
    LOG_ENT();
    RC rc;

    XhciTrb * pTrb;
    CHECK_RC(InsertEmptyTrb(&pTrb));
    CHECK_RC(TrbHelper::SetupCmdResetEndp(pTrb, SlotId, EndPoint, IsTransferStatePreserve));

    if ( IsIssue )
        CHECK_RC(m_pHost->RingDoorBell());

    if ( WaitMs != 0 )
        CHECK_RC(m_pHost->WaitCmdCompletion());

    LOG_EXT();
    return OK;
}

RC CmdRing::IssueStopEndp(UINT08 SlotId,
                          UINT08 Dci,
                          bool IsSp,
                          bool IsIssue,
                          UINT32 WaitMs)
{
    LOG_ENT();
    RC rc;

    XhciTrb * pTrb;
    CHECK_RC(InsertEmptyTrb(&pTrb));
    CHECK_RC(TrbHelper::SetupCmdStopEndp(pTrb, SlotId, Dci, IsSp));

    if ( IsIssue )
        CHECK_RC(m_pHost->RingDoorBell());

    if ( WaitMs != 0 )
        CHECK_RC(m_pHost->WaitCmdCompletion());

    LOG_EXT();
    return OK;

}

RC CmdRing::IssueSetDeqPtr(UINT08 SlotId,
                           UINT08 Dci,
                           PHYSADDR pPaDeq,
                           bool IsDcs,
                           UINT08 StreamId,
                           UINT08 Sct,
                           bool IsIssue,
                           UINT32 WaitMs)
{
    LOG_ENT();
    RC rc;

    XhciTrb * pTrb;
    CHECK_RC(InsertEmptyTrb(&pTrb));
    CHECK_RC(TrbHelper::SetupCmdSetDeq(pTrb, pPaDeq, IsDcs, SlotId, Dci, StreamId, Sct));

    if ( IsIssue )
        CHECK_RC(m_pHost->RingDoorBell());

    if ( WaitMs != 0 )
        CHECK_RC(m_pHost->WaitCmdCompletion());

    LOG_EXT();
    return OK;
}

RC CmdRing::IssueResetDev(UINT08 SlotId,
                          bool IsIssue,
                          UINT32 WaitMs)
{
    LOG_ENT();
    RC rc;

    XhciTrb * pTrb;
    CHECK_RC(InsertEmptyTrb(&pTrb));
    CHECK_RC(TrbHelper::SetupCmdResetDev(pTrb, SlotId));

    if ( IsIssue )
        CHECK_RC(m_pHost->RingDoorBell());

    if ( WaitMs != 0 )
        CHECK_RC(m_pHost->WaitCmdCompletion());

    LOG_EXT();
    return OK;

}

RC CmdRing::IssueGetPortBandwidth(void * pPortBwContext,
                                  UINT08 DevSpeed,
                                  UINT08 HubSlotId,
                                  bool IsIssue /* = true */,
                                  UINT32 WaitMs /* = 2000 */)
{
    LOG_ENT();
    RC rc;

    XhciTrb * pTrb;
    CHECK_RC(InsertEmptyTrb(&pTrb));

    PHYSADDR pPa = Platform::VirtualToPhysical(pPortBwContext);
    CHECK_RC(TrbHelper::SetupCmdGetPortBw(pTrb, pPa, DevSpeed, HubSlotId));

    if ( IsIssue )
        CHECK_RC(m_pHost->RingDoorBell());

    if ( WaitMs != 0 )
        CHECK_RC(m_pHost->WaitCmdCompletion());

    LOG_EXT();
    return OK;
}

RC CmdRing::IssueForceHeader(UINT32 Lo,
                             UINT32 Mid,
                             UINT32 Hi,
                             UINT08 Type,
                             UINT08 PortNum,
                             bool IsIssue /* = true */,
                             UINT32 WaitMs /* = 2000 */)
{
    LOG_ENT();
    RC rc;

    XhciTrb * pTrb;
    CHECK_RC(InsertEmptyTrb(&pTrb));

    CHECK_RC(TrbHelper::SetupCmdForceHeader(pTrb,Lo,Mid,Hi,Type,PortNum));

    if ( IsIssue )
        CHECK_RC(m_pHost->RingDoorBell());

    if ( WaitMs != 0 )
        CHECK_RC(m_pHost->WaitCmdCompletion());

    LOG_EXT();
    return OK;
}

RC CmdRing::IssueGetExtndProperty(UINT08 SlotID,
                                  UINT08 EpId,
                                  UINT08 SubType,
                                  UINT16 Eci,
                                  void * pExtndPropContext /* = nullptr*/,
                                  bool IsIssue/* = true*/,
                                  UINT32 WaitMs /*= 2000*/)
{
    LOG_ENT();
    RC rc;

    XhciTrb * pTrb;
    CHECK_RC(InsertEmptyTrb(&pTrb));

    PHYSADDR pPa = 0;
    if (pExtndPropContext)
    {
        pPa = Platform::VirtualToPhysical(pExtndPropContext);
    }

    CHECK_RC(TrbHelper::SetupCmdGetExtendedProperty(pTrb,
                                                    SlotID,
                                                    EpId,
                                                    SubType,
                                                    Eci,
                                                    pPa));

    // issue a device command
    if ( IsIssue )
        CHECK_RC(m_pHost->RingDoorBell());

    // wait for the commnad completion event
    if ( WaitMs != 0 )
        CHECK_RC(m_pHost->WaitCmdCompletion());

    LOG_EXT();
    return OK;
}

// Event Ring
//==============================================================================
EventRing::EventRing(XhciComnDev * pHost, UINT32 Index, UINT32 ErstSize)
{
    LOG_ENT();
    m_RingType      = XHCI_RING_TYPE_EVENT;

    m_pHost         = pHost;
    m_Index         = Index;
    m_ErstSize      = ErstSize;

    m_pEventRingSegTbl      = NULL;
    m_DeqPending            = 0;
    m_IsResolveNeeded       = false;
    m_IsUsingCompletionCode = false;

    m_RingAlignment = XHCI_ALIGNMENT_EVENT_RING;

    m_vEventQueue.clear();
    LOG_EXT();
}

/*virtual*/ EventRing::~EventRing()
{
    LOG_ENT();
    if ( m_pEventRingSegTbl )
    {
        MemoryTracker::FreeBuffer(m_pEventRingSegTbl);
    }
    m_vEventQueue.clear();
    LOG_EXT();
}

RC EventRing::Init(UINT32 NumElem, UINT32 AddrBit)
{
    RC rc;
    if ( !m_pHost )
    {
        Printf(Tee::PriError, "pHost NULL pointer \n");
        return RC::BAD_PARAMETER;
    }
    // allocate memory for ERST
    MemoryTracker::AllocBufferAligned(XHCI_RING_ERST_ENTRY_SIZE * m_ErstSize,
                                      (void**)&m_pEventRingSegTbl,
                                      XHCI_ALIGNMENT_ERST,
                                      AddrBit,
                                      Memory::UC);
    if ( !m_pEventRingSegTbl )
    {
        Printf(Tee::PriError, "Fail to allocate memory for ESRT \n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }
    Memory::Fill32(m_pEventRingSegTbl, 0, XHCI_RING_ERST_ENTRY_SIZE * m_ErstSize / 4);

    CHECK_RC(AppendTrbSeg(NumElem, AddrBit));
    Printf(Tee::PriLow, "-- Event Ring %d init\n", m_Index);
    return OK;
}

// CycleBit: 0- don't touch; 1 set; -1 clear
RC EventRing::SetDeqPtr(UINT32 SegIndex, UINT32 TrbIndex, INT32 CycleBit)
{
    RC rc;
    CHECK_RC(XhciTrbRing::SetPtr(SegIndex, TrbIndex, false));
    CHECK_RC(m_pHost->UpdateEventRingDeq(m_Index, m_pDeqPtr, m_LwrrentSegment));
    //CHECK_RC(UpdateRegisterDeqPtr());
    if ( 0 != CycleBit )
    {
        m_CycleBit = CycleBit > 0;
    }
    return OK;
}

bool EventRing::CheckXferEvent(UINT08 SlotId, UINT08 Dci, XhciTrb *pTrb)
{
    RC rc = WaitForEvent(XhciTrbTypeEvTransfer,
                         0,
                         pTrb,
                         false,
                         SlotId,
                         Dci);
    if ( OK != rc )
    {
        return false;
    }
    return true;
}

bool EventRing::GetNewEvent(XhciTrb *pTrb, UINT32 TimeoutMs)
{
    if ( !pTrb )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return false;
    }
    GetEvent(TimeoutMs);
    if ( m_vEventQueue.size() == 0 )
    {
        return false;
    }
    TrbHelper::TrbCopy(pTrb, &(m_vEventQueue[0]),
                       XHCI_TRB_COPY_DEST_IN_STACK|XHCI_TRB_COPY_SRC_IN_STACK);
    // delete the event when retrieving
    m_vEventQueue.erase(m_vEventQueue.begin() + 0);
    return true;
}

RC EventRing::FlushRing(UINT32 * pNumEvent)
{
    LOG_ENT();
    RC rc;
    CHECK_RC(GetEvent(0)); // collect Event TRBs into local queue
    if ( pNumEvent )
    {   // count the event in the local queue
        *pNumEvent = (UINT32)m_vEventQueue.size();
    }
    else
    {
        if ( m_vEventQueue.size() )
        {
            Printf(Tee::PriLow, "  %u Events flushed \n", (UINT32)m_vEventQueue.size());
        }
    }

    if ( m_vEventQueue.size() )
    {   // info dump for debug
        for ( UINT32 i = 0; i<m_vEventQueue.size(); i++ )
        {   //dump the event TRBs for debug
            TrbHelper::DumpTrbInfo(&(m_vEventQueue[i]), true, Tee::PriDebug);
        }
    }
    // delete all bufferred events
    m_vEventQueue.clear();
    LOG_EXT();
    return OK;
}

RC EventRing::PrintInfo(Tee::Priority Pri)
{
    if ( !m_pEventRingSegTbl )
    {
        Printf(Tee::PriError, "Event Ring Segment Table has not been initialized \n");
        return RC::WAS_NOT_INITIALIZED;
    }
    PrintTitle(Pri);
    // Print the Event Ring Segment Table
    Printf(Pri, "Event Ring Segment Table:\n");
    for ( UINT32 i = 0; i < m_TrbSeglist.size(); i++ )
    {//
        if ( !FLD_TEST_RF_NUM(XHCI_ERST_DW2_SIZE, 0, MEM_RD32(&m_pEventRingSegTbl[i].DW2)) )  // size != 0
            Printf(Pri, "Event Segment %d Base 0x%08x%08x, Size %d\n",
                   i,
                   RF_VAL(XHCI_ERST_DW0_BASE_LO, MEM_RD32(&m_pEventRingSegTbl[i].DW1)) << (0?XHCI_ERST_DW0_BASE_LO),
                   RF_VAL(XHCI_ERST_DW1_BASE_HI, MEM_RD32(&m_pEventRingSegTbl[i].DW0)) << (0?XHCI_ERST_DW1_BASE_HI),
                   RF_VAL(XHCI_ERST_DW2_SIZE, MEM_RD32(&m_pEventRingSegTbl[i].DW2)));
        else
            break;
    }
    return XhciTrbRing::PrintInfo(Pri);
}

RC EventRing::PrintTitle(Tee::Priority Pri)
{
    Printf(Pri, "Event Ring %u \n", m_Index);
    return OK;
}

RC EventRing::PrintERST(Tee::Priority Pri)
{
    Printf(Pri, "Event Ring %u Segment Table (Virt %p, Phys 0x%llx): \n",
           m_Index,
           m_pEventRingSegTbl,
           Platform::VirtualToPhysical(m_pEventRingSegTbl));
    for ( UINT32 i = 0; i < m_TrbSeglist.size(); i++ )
    {
        Printf(Pri, "  0x%08x 0x%08x 0x%08x 0x%08x \n",
               MEM_RD32(&m_pEventRingSegTbl[i].DW0),
               MEM_RD32(&m_pEventRingSegTbl[i].DW1),
               MEM_RD32(&m_pEventRingSegTbl[i].DW2),
               MEM_RD32(&m_pEventRingSegTbl[i].DW3));
    }
    Printf(Pri, "\n");
    return OK;
}

RC EventRing::GetERSTBA(PHYSADDR * ppERSTBA, UINT32 * pSize)
{
    if ( !ppERSTBA || !pSize )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }

    *ppERSTBA = Platform::VirtualToPhysical(m_pEventRingSegTbl);
    * pSize = m_TrbSeglist.size();

    if ( (*ppERSTBA) & ((1<<(0?XHCI_REG_ERSTBA_BITS)) - 1) )
    {
        Printf(Tee::PriError, "Event Segment Table bad alignment \n");
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

RC EventRing::GetSegmentBase(UINT32 SegIndex, PHYSADDR * ppSegment)
{
    if ( !ppSegment )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    if (SegIndex >= m_TrbSeglist.size())
    {
        Printf(Tee::PriError, "Invalid Segment Index %d, valid [0 - %d-1] \n", SegIndex, (UINT32) m_TrbSeglist.size());
        return RC::BAD_PARAMETER;
    }
    * ppSegment = Platform::VirtualToPhysical(m_TrbSeglist[SegIndex].Address);
    return OK;
}

RC EventRing::AppendTrbSeg(UINT32 NumElem, UINT32 AddrBit)
{
    LOG_ENT();
    RC rc;

    if ( m_TrbSeglist.size() >= m_ErstSize )
    {
        Printf(Tee::PriError, "Event Segment Table Full, lwrent size %d \n", m_ErstSize);
        return RC::OUT_OF_HW_RSC;
    }
/*    if ( m_DeqPending != 0 )
    {
        Printf(Tee::PriError, "A segment is pending, call AppendTrbSeg() later");
        return RC::UNSUPPORTED_FUNCTION;
    } */

    // allocate memory and update vector
    CHECK_RC(XhciTrbRing::AppendTrbSeg(NumElem, -1, false, AddrBit));

    // update the segment base table entry
    UINT32 pendingIndex = m_TrbSeglist.size() - 1;
    // get the physical address of the new segment
    XhciTrb * pHead;
    CHECK_RC(GetTrb(pendingIndex, 0, &pHead));
    PHYSADDR pPa = Platform::VirtualToPhysical((void*)pHead);
    MEM_WR32(&(m_pEventRingSegTbl[pendingIndex].DW0), (UINT32)pPa);
    pPa = pPa >> 32;
    MEM_WR32(&(m_pEventRingSegTbl[pendingIndex].DW1), (UINT32)pPa);
    MEM_WR32(&(m_pEventRingSegTbl[pendingIndex].DW2), FLD_SET_RF_NUM(XHCI_ERST_DW2_SIZE, NumElem, MEM_RD32(&(m_pEventRingSegTbl[pendingIndex].DW2))));
    MEM_WR32(&(m_pEventRingSegTbl[pendingIndex].DW3), 0);

    // sign a signal, record the index of first pending segment
    if ( 0 == m_DeqPending )
    {
        m_DeqPending = pendingIndex;
    }
    // write the size of event ring segment table
    CHECK_RC(m_pHost->EventRingSegmengSizeHandler(m_Index, (UINT32) m_TrbSeglist.size()));

    LOG_EXT();
    return OK;
}

RC EventRing::RemoveTrbSeg()
{
    LOG_ENT();
    RC rc;

    if ( m_TrbSeglist.size() == 1 )
    {
        Printf(Tee::PriError, "At lest one segment needed for Event Ring \n");
        return RC::BAD_PARAMETER;
    }
    if ( m_DeqPending != 0 )
    {
        Printf(Tee::PriError, "A segment is pending, call RemoveTrbSeg() later \n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    // sign a signal, the real deletion will happen at GetEvent()
    // after we find out the current Enq Pointer and make sure the segment removing
    // won't cause any HW bad behaviour
    m_DeqPending = -(m_TrbSeglist.size() - 1);

    // write the register
    CHECK_RC(m_pHost->EventRingSegmengSizeHandler(m_Index, (UINT32) m_TrbSeglist.size() - 1));
    // CHECK_RC(m_pHost->RegWr(XHCI_REG_ERSTSZ(m_Index), m_TrbSeglist.size() - 1));

    LOG_EXT();
    return OK;
}

// Keep gathering event TRBs till any of two conditions oclwrs
// 1. No more Event Trb available
// 2. the Enq route is pending (in the case Event Segment Table size has been changed prior to entering this function)
RC EventRing::GetEvent(UINT32 TimeoutMs,
                       UINT32* pNumEvent /* = NULL */)
{
    LOG_ENT();
    Printf(Tee::PriDebug,"(TimeoutMs = %d)\n", TimeoutMs);
    RC rc;
    UINT32 numEvents = 0;
    CHECK_RC(VerifyPointer());

    if ( m_IsResolveNeeded )
    { // we need to find out the EnqPtr path
        UINT32 ms = 0;
        do
        {
            bool isResolved = false;
            CHECK_RC(SolveEnqRoute(m_DeqPending, &isResolved));
            if ( isResolved )
            {
                m_IsResolveNeeded = false;
                break;
            }

            if ( TimeoutMs )
            {
                Platform::SleepUS(10*1000);
                ms++;
            }
        } while ( (ms*10) < TimeoutMs );
        if ( ms >= TimeoutMs )
        {
            if ( TimeoutMs != 0 )
                return RC::TIMEOUT_ERROR;
            else
                return OK;
        }
    }

    // by default, we won't get into Deq pending condition unless Segment append/remove
    while ( true )
    {   // keep gathering Event TRBs
        Printf(Tee::PriDebug,
               "  Now probing %p, %d of Seg %d(Event %u), with %s ",
               m_pDeqPtr, m_LwrrentIndex, m_LwrrentSegment, m_Index, m_IsUsingCompletionCode?"Completion Code":"Cycle Bit");
        if ( m_IsUsingCompletionCode )
            Printf(Tee::PriDebug, " \n");
        else
            Printf(Tee::PriDebug, " %d \n", m_CycleBit?1:0);

        if ( TimeoutMs )
        {
            XhciComnDev::IntrModes intrMode;
            CHECK_RC(m_pHost->GetInterruptMode(&intrMode));
            Printf(Tee::PriDebug, "\tInterrupt Mode applied: 0x%x \n", (UINT32)intrMode);

            Tasker::EventID eventId;
            CHECK_RC(m_pHost->GetEventId(m_Index, &eventId));

            if ( XhciComnDev::POLL == intrMode )
            {
                if ( !m_IsUsingCompletionCode )
                {
                    rc = m_pHost->WaitMem((void *)&(m_pDeqPtr->DW3), m_CycleBit?0x1:0, m_CycleBit?0:0x1, TimeoutMs);
                }
                else
                {
                    rc = m_pHost->WaitBitSetAnyMem((void *)&(m_pDeqPtr->DW2), RF_SHIFTMASK(XHCI_TRB_EVENT_DW2_COMPLETION_CODE), TimeoutMs);
                }
            }
            else
            {
                rc = Tasker::WaitOnEvent(eventId, TimeoutMs);
                if ( OK == rc )
                    Tasker::ResetEvent(eventId);
            }

            if ( (OK == rc) && ( XhciComnDev::IRQ == intrMode) )
            {   // this is a protection.
                // sometimes when IRQ turned on for the first time, an interrupt
                // will be generated no matter if Event Ring is empty
                if ( !m_IsUsingCompletionCode )
                {
                    if ( FLD_TEST_RF_NUM(XHCI_TRB_DW3_CYCLE, m_CycleBit?0:1, MEM_RD32(&m_pDeqPtr->DW3)) )
                    {
                        Printf(Tee::PriWarn, "Bogus Event has been removed \n");
                        rc = RC::TIMEOUT_ERROR; // emulate a timeout
                    }
                }
                else
                {
                    if ( FLD_TEST_RF_NUM(XHCI_TRB_EVENT_DW2_COMPLETION_CODE, 0, MEM_RD32(&m_pDeqPtr->DW2)) )
                    {
                        Printf(Tee::PriWarn, "Bogus Event has been removed \n");
                        rc = RC::TIMEOUT_ERROR; // emulate a timeout
                    }
                }
            }
        }
        else
        {
            bool cBit;
            if ( !m_IsUsingCompletionCode )
            {
                CHECK_RC(TrbHelper::GetCycleBit(m_pDeqPtr, &cBit));
                if ( cBit == m_CycleBit )
                    rc = OK;   // have one more
                else
                    rc = RC::TIMEOUT_ERROR; // emulate a timeout
            }
            else
            {
                if ( RF_VAL(XHCI_TRB_EVENT_DW2_COMPLETION_CODE, MEM_RD32(&m_pDeqPtr->DW2)) )
                    rc = OK;
                else
                    rc = RC::TIMEOUT_ERROR; // emulate a timeout
            }
        }

        if ( RC::TIMEOUT_ERROR == rc )
        {   // no Event available, Deq = Enq, ring is empty
            rc.Clear();     // Restore RC to OK;
            break;
        }

        // save the TRB at m_pDeqPtr to local event queue
        TimeoutMs = 0;  // we only use wait for the first TRB, once we get one, we exit immediately when ring get empty
        XhciTrb tmpTrb;
        CHECK_RC(TrbHelper::TrbCopy(&tmpTrb, m_pDeqPtr, XHCI_TRB_COPY_DEST_IN_STACK));
        m_vEventQueue.push_back(tmpTrb);
        numEvents++;
        // This is the hook place. all the Event related hacks goes here.
        // Hook 1:
        // once we get a new Event, we should check if it's a Command Completion TRB
        // if it is, we should update the DeqPtr for command Ring
        // all command completion goes to Event Ring 0
        CHECK_RC(m_pHost->NewEventTrbHandler(&tmpTrb));
        // TRB collected, now advance the Deq Pointer
        if ( m_LwrrentIndex < (m_TrbSeglist[m_LwrrentSegment].ByteSize / XHCI_TRB_LEN_BYTE - 1) )
        {   // there's enough Event TRB slot left in current segment
            m_LwrrentIndex++;
            m_pDeqPtr++;
        }
        else
        {
            // not enough TRB slots left, move to the next segment, 3 situations
            Printf(Tee::PriLow, "  Event Ring DeqPtr is crossing segment %u \n", m_LwrrentSegment);
            if ( m_IsUsingCompletionCode )
                m_IsUsingCompletionCode = false;

            if ( m_LwrrentSegment == (m_TrbSeglist.size()-1) )
            { // 1: this is the last segment, toggle the m_CycleBit, we are going to roll over
                m_CycleBit = !m_CycleBit;
                Printf(Tee::PriLow, "Cycle bit toggled, = %u\n", m_CycleBit?1:0);

                m_LwrrentSegment = 0;
                m_LwrrentIndex = 0;
                CHECK_RC(GetTrb(0, 0, &m_pDeqPtr));
                //m_pDeqPtr = (XhciTrb*) m_TrbSeglist[0].Address;
            }
            else if ( (m_DeqPending != 0)
                      && (m_LwrrentSegment == (Abs(m_DeqPending)-1 )) )
            { // 2: we are facing the route pending
                bool isSolved;
                m_IsResolveNeeded = false;
                CHECK_RC(SolveEnqRoute(m_DeqPending, &isSolved));
                if ( !isSolved )
                {
                    m_IsResolveNeeded = true;
                    break;    // we couldn't advance any further
                }
            }
            else
            {  // 3: the rest
                m_LwrrentSegment++;
                m_LwrrentIndex = 0;
                CHECK_RC(GetTrb(m_LwrrentSegment, 0, &m_pDeqPtr));
                //m_pDeqPtr = (XhciTrb*) m_TrbSeglist[m_LwrrentSegment].Address;
            }
        }
    }

    if (numEvents)
    {
        // CHECK_RC(UpdateRegisterDeqPtr());
        CHECK_RC(m_pHost->UpdateEventRingDeq(m_Index, m_pDeqPtr, m_LwrrentSegment));
    }

    Printf(Tee::PriDebug, "%u Events retrieved", numEvents);
    LOG_EXT();
    if ( pNumEvent )
    {
        *pNumEvent = numEvents;
    }
    return OK;  // <-- return OK is intentionaly
}

// this is the implementation of spec 4.9.4.1 Changing the size of an Event Ring
// when Deq get stuck, this function will try to resolve it. return true if resolved
// in this case, the m_DeqPtr will also be advanced
RC EventRing::SolveEnqRoute(INT32 PendingSegment, bool * pIsSolved)
{
    Printf(Tee::PriDebug, "EventRing::SolveEnqRoute(Pending Seg %d)\n", PendingSegment);
    if ( !pIsSolved )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    *pIsSolved = false;

    RC rc;
    bool isAdding = true;
    if ( PendingSegment < 0 )
    {
        PendingSegment = -PendingSegment;
        isAdding = false;
    }

    CHECK_RC(VerifyPointer());
    if ( m_LwrrentSegment != (Abs(PendingSegment)-1)
         || (m_LwrrentIndex != m_TrbSeglist[m_LwrrentSegment].ByteSize/XHCI_TRB_LEN_BYTE-1) )
    {
        Printf(Tee::PriError, "It's not in Deq pending condition, should not call me \n");
        return RC::SOFTWARE_ERROR;
    }

    if ( isAdding )
    {   // enlarging. I think spec left some defect here.
        // as the MFINDEX Wrap Event TRB doesn't have a Completion Code field
        XhciTrb *pTrb = (XhciTrb *)m_TrbSeglist[PendingSegment].Address;
        UINT08 compCode;
        CHECK_RC(TrbHelper::GetCompletionCode(pTrb, &compCode, true, false));
        if ( 0 != compCode )
        {   // found TRB on newly added segment
            m_pDeqPtr = (XhciTrb *)m_TrbSeglist[PendingSegment].Address;
            m_LwrrentSegment = PendingSegment;
            m_LwrrentIndex = 0;
            *pIsSolved = true;
            Printf(Tee::PriLow, "Adding: Solved at Seg %d \n", PendingSegment);
            // switch to the second method of EnqPtr detection
            m_IsUsingCompletionCode = true;
            m_DeqPending = 0;
        }
        if ( (*pIsSolved) != true )
        {
            // SW shall check the state of the Cycle bit in the first TRB of segment 0 to see
            // whether it matches the expected state
            pTrb = (XhciTrb *)m_TrbSeglist[0].Address;
            bool cBit;
            CHECK_RC(TrbHelper::GetCycleBit(pTrb, &cBit));
            if ( cBit != m_CycleBit )
            {   // found TRB on first segment
                m_pDeqPtr = (XhciTrb *)m_TrbSeglist[0].Address;
                m_LwrrentSegment = 0;
                m_LwrrentIndex = 0;
                m_CycleBit = !m_CycleBit;
                *pIsSolved = true;
                Printf(Tee::PriLow, "Adding: Solved at Seg 0 \n");
            }
        }
    }
    else
    {   // shrinking
        XhciTrb *pTrb = (XhciTrb *)m_TrbSeglist[PendingSegment].Address;
        bool cBit;
        CHECK_RC(TrbHelper::GetCycleBit(pTrb, &cBit));
        if ( cBit == m_CycleBit )
        {   // found TRB on deleted segment
            m_pDeqPtr = (XhciTrb *)m_TrbSeglist[PendingSegment].Address;
            m_LwrrentSegment = PendingSegment;
            m_LwrrentIndex = 0;
            *pIsSolved = true;
            Printf(Tee::PriLow, "Shrinking: Solved at Seg %d \n", PendingSegment);
        }
        if ( (*pIsSolved) != true )
        {
            pTrb = (XhciTrb *)m_TrbSeglist[0].Address;
            CHECK_RC(TrbHelper::GetCycleBit(pTrb, &cBit));
            if ( cBit != m_CycleBit )
            {   // found TRB on first segment
                m_pDeqPtr = (XhciTrb *)m_TrbSeglist[0].Address;
                m_LwrrentSegment = 0;
                m_LwrrentIndex = 0;
                m_CycleBit = !m_CycleBit;
                *pIsSolved = true;
                Printf(Tee::PriLow, "Shrinking: Solved at Seg 0 \n");
                XhciTrbRing::RemoveTrbSeg(m_TrbSeglist.size()-1, false); // release the resource
                m_DeqPending = 0;
            }
        }
    }

    return OK;
}

// Type: 0-Trb Address, 1-Trb Type
RC EventRing::WaitForEventPortal(UINT08 EventType,
                                 void * Ptr,
                                 UINT32 WaitMs,
                                 XhciTrb * pTrb,
                                 bool IsFlushEvents,
                                 UINT08 SlotId,
                                 UINT08 Dci,
                                 bool IsExitOnError)
{
    LOG_ENT();
    if ( (!Ptr && !EventType)
         || (Ptr && EventType ) )
    {
        Printf(Tee::PriError, "EventType = %d, TRB pointer = %p \n", EventType, Ptr);
        return RC::SOFTWARE_ERROR;
    }
    if ( 0 == EventType )
    {
        Printf(Tee::PriDebug,"Wait for TRB %p, Slot %d, Dci %d\n", Ptr, SlotId, Dci);
    }
    else
    {
        Printf(Tee::PriDebug,"Wait for Type %d, Slot %d, Dci %d\n", EventType, SlotId, Dci);
        if ( SlotId
             && ( EventType != XhciTrbTypeEvTransfer ) )
        {
            Printf(Tee::PriError, "SlotId could only be used with Xfer Event TRB \n");
            return RC::BAD_PARAMETER;
        }
    }

    RC rc = OK;
    if ( WaitMs )
    {
        XhciController::GetTimeout(& WaitMs); // adopt user specified timeout
    }
    UINT32 obsoleteEvent = 0;       // record the Event number already parsed
    bool isFound = false;           // if the expected Event has been found
    bool isErrorHappen = false;     // apply only to search by address

    UINT64 timeStart = Platform::GetTimeUS();
    UINT64 timeElapse;
    UINT32 msLeft = WaitMs;

    UINT08 trbType;
    UINT32 index = 0;

    // we should first use the Events in local queue, no need to wait for new Event
    bool isLocalQueueCount = true;
    do
    {
        CHECK_RC( GetEvent(isLocalQueueCount?0:msLeft) ); // collect Event TRBs into local event queue - m_vEventQueue
        isLocalQueueCount = false;

        //dump Event for debug
        for ( index = obsoleteEvent; index < m_vEventQueue.size(); index++ )
        {
            Printf(Tee::PriDebug, "---- New event @ %d: \n" , index);
            TrbHelper::DumpTrb(&(m_vEventQueue[index]), Tee::PriDebug, true, true);
        }

        if ( 0 == EventType )
        {   // search by address
            PHYSADDR pPa = Platform::VirtualToPhysical(Ptr);
            PHYSADDR pPHY = 0;
            Printf(Tee::PriDebug, "  screening Events for TRB 0x%llx\n", pPHY);
            for ( index = obsoleteEvent; index < m_vEventQueue.size(); index++ )
            {
                CHECK_RC(TrbHelper::GetTrbType(&(m_vEventQueue[index]), &trbType, Tee::PriDebug, true));
                if ( trbType == XhciTrbTypeEvTransfer
                     ||trbType == XhciTrbTypeEvCmdComplete )
                {
                    UINT08 completionCode;
                    CHECK_RC(TrbHelper::GetCompletionCode(&(m_vEventQueue[index]), &completionCode, false, true));

                    if ( pTrb )
                    {   // if STALL oclwrs during waiting for Event TRB, record it and return
                        // upper functions will handle it
                        if ( XhciTrbCmpCodeStallErr == completionCode
                             || XhciTrbCmpCodeBabbleDetectErr == completionCode )
                        {   // STALL & Babble, Endpoint halts, exit immediatly
                            Printf(Tee::PriLow, "  completion code %d, need EP reset \n", completionCode);
                            isFound = true;
                            break;
                        }
                    }

                    CHECK_RC(TrbHelper::ParseTrbPointer(&(m_vEventQueue[index]), &pPHY));
                    if ( pPa == pPHY )
                    {   // found it
                        Printf(Tee::PriDebug, "Event TRB %p Found\n", Ptr);
                        isFound = true;
                        break;
                    }
                    else
                    {   // check if something bad happens in the middle of waiting for the last TRB
                        if ( XhciTrbCmpCodeSuccess != completionCode )
                        {
                            if ( IsExitOnError )
                            {
                                Printf(Tee::PriError,
                                       "Abnormal Completion Code %d \n",
                                       completionCode);
                                isErrorHappen = true;
                                isFound = true;
                                break;
                            }
                            else
                            {
                                Printf(Tee::PriWarn,
                                       "  Abnormal Completion Code %d \n",
                                       completionCode);
                            }
                        }
                    }   // if expected address not found
                }
            }
        }
        else
        {   // search by type
            Printf(Tee::PriDebug, "  screening Events for Type %d\n", EventType);
            for ( index = obsoleteEvent; index < m_vEventQueue.size(); index++ )
            {
                CHECK_RC(TrbHelper::GetTrbType(&(m_vEventQueue[index]), &trbType, Tee::PriDebug, true));
                if ( trbType == EventType )
                {
                    if ( SlotId )
                    {   // we need to do more check
                        UINT08 slotId, dci, dummy;
                        XhciTrb myTrb;
                        CHECK_RC(TrbHelper::TrbCopy(&myTrb,  &(m_vEventQueue[index]),
                                                    XHCI_TRB_COPY_DEST_IN_STACK|XHCI_TRB_COPY_SRC_IN_STACK));
                        // TrbHelper::ParseXxx() takes only variables in stack, not in heap
                        CHECK_RC(TrbHelper::ParseXferEvent(&myTrb, &dummy, NULL, NULL, NULL, &slotId, &dci));
                        if ( SlotId == slotId && Dci == dci )
                        {
                            isFound = true;
                        }
                    }
                    else
                    {   // no slotid match required
                        isFound = true;
                    }
                    if ( isFound )
                    {
                        Printf(Tee::PriDebug, "Event Type %d Found \n", EventType);
                        break;
                    }
                }
            }
        }
        if ( isFound && pTrb )
        {
            CHECK_RC(TrbHelper::TrbCopy(pTrb,  &(m_vEventQueue[index]),
                                        XHCI_TRB_COPY_DEST_IN_STACK|XHCI_TRB_COPY_SRC_IN_STACK));
        }

        obsoleteEvent =  m_vEventQueue.size();

        timeElapse = Platform::GetTimeUS() - timeStart;
        if (timeElapse < WaitMs * 1000ULL)
            msLeft = WaitMs - timeElapse /1000;
        else
            msLeft = 0;
    }
    while ( !isFound && (msLeft > 10) );

    if ( !isFound )
    {
        rc = RC::TIMEOUT_ERROR;
        if ( WaitMs > 100  )
        {   // constrain the error messages to prevent junk log when frequency > 10Hz
            if ( 0 == EventType )
                Printf(Tee::PriError, "Wait for TRB 0x%llx(virt:%p) timeout \n", Platform::VirtualToPhysical(Ptr), Ptr);
            else
                Printf(Tee::PriError, "Wait for Event Type %i timeout \n", EventType);
        }

        if ( m_vEventQueue.size() )
        {
            Printf(Tee::PriLow, "  %d un-expected Events in Queue: \n", (UINT32)m_vEventQueue.size());
            for ( UINT32 i = 0; i<m_vEventQueue.size(); i++ )
            { //dump the event TRBs for debug
                TrbHelper::DumpTrbInfo(&(m_vEventQueue[i]), true, Tee::PriLow);
            }
        }
    }
    else
    {
        Printf(Tee::PriLow, " Expected Event TRB Found - ");
        TrbHelper::DumpTrbInfo(&(m_vEventQueue[index]), true, Tee::PriLow);
    }

    if ( IsFlushEvents )
    {
        if ( m_vEventQueue.size() )
        {
            Printf(Tee::PriLow, "  %d Events removed from queue \n", (UINT32)m_vEventQueue.size());
            m_vEventQueue.clear();
        }
    }
    else    // delete the returned event to prevent redanduncy
    {
        if ( isFound )
        {
            m_vEventQueue.erase(m_vEventQueue.begin() + index);
        }
    }

    if ( isErrorHappen )
    {
        rc = RC::HW_STATUS_ERROR;
    }
    LOG_EXT();
    return rc;
}

RC EventRing::WaitForEvent(UINT08 EventType,
                           UINT32 WaitMs,
                           XhciTrb *pEventTrb,
                           bool IsFlushEvents,
                           UINT08 SlotId, /* =0 */
                           UINT08 Dci/* =0 */)
{
    return WaitForEventPortal(EventType, NULL, WaitMs, pEventTrb, IsFlushEvents, SlotId, Dci);
}

RC EventRing::WaitForEvent(XhciTrb * pTrb,
                           UINT32 WaitMs,
                           XhciTrb *pEventTrb,
                           bool IsFlushEvents,
                           bool IsExtOnError /* = false*/)
{
    return WaitForEventPortal(0, (void*)pTrb, WaitMs, pEventTrb, IsFlushEvents, 0, 0, IsExtOnError);
}

// Xfer Ring
//==============================================================================
TransferRing::TransferRing(UINT08 SlotId,
                           UINT08 Endpoint,
                           bool IsOut,
                           UINT16 StreamId,
                           bool IsEp0HasDir)
{
    LOG_ENT();
    m_RingType  = XHCI_RING_TYPE_XFER;
    m_SlotId    = SlotId;
    m_EndPoint  = Endpoint;
    m_IsDataOut = IsOut;
    m_StreamId  = StreamId;
    m_IsEp0HasDir   = IsEp0HasDir;

    m_IntrTarget= 0;
    m_IsIoc     = true;
    m_vpBuffer.clear();
    m_FragList.clear();

    m_RingAlignment = XHCI_ALIGNMENT_XFER_RING;
    LOG_EXT();
}

/*virtual*/ TransferRing::~TransferRing()
{
    LOG_ENT();
    BufferRelease();
    LOG_EXT();
}

RC TransferRing::Init(UINT32 NumElem, UINT32 AddrBit)
{
    RC rc;
    CHECK_RC(AppendTrbSeg(NumElem, AddrBit));  // released in Device Class
    Printf(Tee::PriLow, "-- Transfer Ring init for Slot %d, Ep %d, Data %s\n", m_SlotId, m_EndPoint, m_IsDataOut?"Out":"In");
    return OK;
}

RC TransferRing::PrintInfo(Tee::Priority Pri)
{
    PrintTitle(Pri);
    return XhciTrbRing::PrintInfo(Pri);
}

RC TransferRing::PrintTitle(Tee::Priority Pri)
{
    Printf(Pri, "Transfer Ring on Slot %d, Ep %d", m_SlotId, m_EndPoint);
    if ( (0 != m_EndPoint) || m_IsEp0HasDir )
    Printf(Pri, ", Data %s", m_IsDataOut?"Out":"In");
    if ( m_StreamId )
        Printf(Pri, ", StreamId 0x%x ", m_StreamId);
    Printf(Pri, " \n");
    return OK;
}

RC TransferRing::SetEnqPtr(UINT32 SegIndex, UINT32 TrbIndex, INT32 CycleBit)
{
    if ( 0 != CycleBit )
    {
        m_CycleBit =  CycleBit > 0;
    }
    return XhciTrbRing::SetPtr(SegIndex, TrbIndex, true);
}

RC TransferRing::GetPcs(bool * pIsCBit)
{
    if ( !pIsCBit )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::SOFTWARE_ERROR;
    }
    * pIsCBit = m_CycleBit;
    return OK;
}

RC TransferRing::SetInterruptTarget(UINT32 IntTarget)
{
    // boundary protected by caller
    m_IntrTarget = IntTarget;
    return OK;
}

RC TransferRing::GetInterruptTarget(UINT32* pIntrTarget)
{
    if ( !pIntrTarget )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    *pIntrTarget = m_IntrTarget;
    return OK;
}

RC TransferRing::SetIoc(bool IsIoc)
{
    m_IsIoc = IsIoc;
    return OK;
}

RC TransferRing::AppendTrbSeg(UINT32 NumElem, UINT32 AddrBit)
{
    RC rc;

    if ( m_TrbSeglist.size()>0 )
    {   // check to see if the Link TRB of last segment is owned by HW
        XhciTrb *pTrb;
        CHECK_RC(GetTrb(m_TrbSeglist.size()-1, -1, &pTrb));
        bool isOwnBySw;
        CHECK_RC(IsOwnBySw(pTrb, &isOwnBySw));
        if ( !isOwnBySw )
        {
            Printf(Tee::PriError, "Link TRB to be updated is owned by HW now, try it later \n");
            return RC::UNSUPPORTED_FUNCTION;
        }
    }

    // Insert the new segment at the tail
    CHECK_RC(XhciTrbRing::AppendTrbSeg(NumElem, -1, true, AddrBit));
    // now update the Cycle bits in the newly added segment to be align with its parent node
    if ( m_TrbSeglist.size() > 1 )
    {
        XhciTrb *pLastTrb;
        UINT32 parentIndex;
        parentIndex = m_TrbSeglist.size()-2;

        CHECK_RC(GetTrb(parentIndex, -2, &pLastTrb));
        bool cBit;
        CHECK_RC(TrbHelper::GetCycleBit(pLastTrb, &cBit));
        if ( cBit )
        {   // we need set all Cycle bits to 1 in the new segment
            for ( UINT32 i = 0; i<m_TrbSeglist[parentIndex+1].ByteSize / XHCI_TRB_LEN_BYTE; i++ )
            {
                CHECK_RC(GetTrb(parentIndex+1, i, &pLastTrb));
                CHECK_RC(TrbHelper::SetCycleBit(pLastTrb, true));
            }
        }
    }
    return OK;
}

RC TransferRing::RemoveTrbSeg(UINT32 SegIndex)
{
    RC rc;
    if ( m_TrbSeglist.size()>1 )
    {   // check to see if the Link TRB of given segment is owned by HW
        UINT32 prevSegIndex;
        if ( SegIndex == 0 )
            prevSegIndex = m_TrbSeglist.size() - 1;
        else
            prevSegIndex = SegIndex - 1;

        XhciTrb *pTrb;
        CHECK_RC(GetTrb(prevSegIndex, -1, &pTrb));
        bool isOwnBySw;
        CHECK_RC(IsOwnBySw(pTrb, &isOwnBySw));
        if ( !isOwnBySw )
        {
            Printf(Tee::PriError, "Link TRB to be updated is owned by HW now, try it later \n");
            return RC::UNSUPPORTED_FUNCTION;
        }
    }
    if ( m_TrbSeglist.size() == 1 )
    {
        Printf(Tee::PriLow, "  Removing the only segment of Xfer Ring\n");
    }
    else if ( m_LwrrentSegment == SegIndex )
    {
        Printf(Tee::PriError, "Enq Pointer is in Segment %u, it can not be removed at this point \n", m_LwrrentSegment);
        return RC::BAD_PARAMETER;
    }
    return XhciTrbRing::RemoveTrbSeg(SegIndex, true);
}

bool TransferRing::IsThatMe(UINT08 SlotId,
                            UINT08 Endpoint,
                            bool IsDataOut,
                            UINT16 StreamId,
                            bool IsSkipStreamID)
{
    if ( (Endpoint == 0) && StreamId )
    {   // control endpoint should not has Stream
        Printf(Tee::PriWarn, "Trying to search Xfer Ring: Endpoint = 0 and StreamID = 0x%04x \n", StreamId);
    }
    if ( (Endpoint == 0) && !m_IsEp0HasDir )
    {
        return((m_SlotId == SlotId) && (m_EndPoint == Endpoint));
    }
    else
    {
        if ( IsSkipStreamID )
        {
            return((m_SlotId == SlotId) && (m_EndPoint == Endpoint) && (m_IsDataOut == IsDataOut));
        }
        else
        {
            return((m_SlotId == SlotId) && (m_EndPoint == Endpoint) && (m_IsDataOut == IsDataOut) && (m_StreamId == StreamId));
        }
    }
}

bool TransferRing::IsThatMe(UINT08 SlotId)
{
    return(m_SlotId == SlotId);
}

RC TransferRing::GetHost(UINT08 * pSlotId,
                         UINT08 * pEndpoint,
                         bool * pIsDataOut,
                         UINT16 *pStreamId)
{
    if ( (!pSlotId) || (!pEndpoint) || (!pIsDataOut) )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    *pSlotId    = m_SlotId;
    *pEndpoint  = m_EndPoint;
    *pIsDataOut = m_IsDataOut;
    if ( pStreamId )
    {
        *pStreamId  = m_StreamId;
    }
    return OK;
}

// functions to deal with m_FragList
RC TransferRing::BufferSave(MemoryFragment::FRAGLIST* pFrags, void * pBuffer)
{
    LOG_ENT();
    for ( UINT32  i = 0; i < pFrags->size(); i++ )
    {
        m_FragList.push_back((*pFrags)[i]);
    }

    MemoryFragment::FRAGMENT myFrag = {};
    myFrag.Address = pBuffer;
    myFrag.ByteSize = pFrags->size();
    m_vpBuffer.push_back(myFrag);

    LOG_EXT();
    return OK;
}

RC TransferRing::BufferRetrieve(MemoryFragment::FRAGLIST * pFrags, UINT32 NumBuf)
{
    LOG_ENT();
    if ( !pFrags )
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    if ( ( 0 == m_vpBuffer.size() )
         || ( 0 == m_FragList.size() ) )
    {
        Printf(Tee::PriError, "No Data Buffer exists for this Xfer Ring \n");
        return RC::BAD_PARAMETER;
    }
    if (0 == NumBuf)
    {   // by default Retrieve all
        NumBuf = m_vpBuffer.size();
    }
    else
    {
        if (NumBuf > m_vpBuffer.size())
        {
            Printf(Tee::PriError, "Invalid number %d, valid[1-%d] \n", NumBuf, (UINT32)m_vpBuffer.size());
            return RC::BAD_PARAMETER;
        }
    }

    pFrags->clear();
    UINT32 numFrags = 0;
    for (UINT32 i = 0; i < NumBuf; i++)
    {
        numFrags += m_vpBuffer[i].ByteSize;
    }
    if (numFrags > m_FragList.size())
    {
            Printf(Tee::PriError, "Invalid frag number %d, valid[1-%d], contact MODS team \n", NumBuf, (UINT32)m_FragList.size());
            return RC::SOFTWARE_ERROR;
    }
    for ( UINT32 i = 0 ; i < numFrags; i++ )
    {
        if ( !MemoryTracker::IsValidVirtualAddress(m_FragList[i].Address) )
        {
            Printf(Tee::PriError, "Memory has been released by other routine \n");
            return RC::BAD_PARAMETER;
        }
        pFrags->push_back(m_FragList[i]);
    }
    LOG_EXT();
    return OK;
}

RC TransferRing::BufferRelease(UINT32 NumBuf)
{
    LOG_ENT();

    if (0 == NumBuf)
    {   // by default release all
        NumBuf = m_vpBuffer.size();
    }
    else
    {
        if (NumBuf > m_vpBuffer.size())
        {
            Printf(Tee::PriError, "Invalid number %d, valid[1-%d] \n", NumBuf, (UINT32)m_vpBuffer.size());
            return RC::BAD_PARAMETER;
        }
    }
    for ( UINT32 i = 0; i < NumBuf; i++ )
    {
        MemoryTracker::FreeBuffer( m_vpBuffer[0].Address );
        UINT32 fragNum =  m_vpBuffer[0].ByteSize;
        // remove the item after the memory is released
        m_vpBuffer.erase(m_vpBuffer.begin());
        // remove the associated items in m_FragList from the beginning
        for (UINT32 j = 0; j < fragNum; j++)
        {
            m_FragList.erase(m_FragList.begin());
        }
    }
    /*
    m_vpBuffer.clear();
    m_FragList.clear();
    */
    LOG_EXT();
    return OK;
}

RC TransferRing::Buf2ImmData(UINT08 * pData, UINT32 Length, UINT64 *pImmData)
{
    if (Length > 8)
    {
        Printf(Tee::PriError, "Length %d > 8 \n", Length);
        return RC::BAD_PARAMETER;
    }
    if (!pImmData || !pData)
    {
        Printf(Tee::PriError, "Null Pointer\n");
        return RC::BAD_PARAMETER;
    }
    *pImmData = 0;
    UINT64 tmp64;
    for (UINT32 i = 0; i < Length; i++)
    {
        tmp64 = MEM_RD08(pData+i);
        tmp64 <<= i * 8;
        *pImmData |= tmp64;
    }
    return OK;
}

// insert one or more Normal TRB
RC TransferRing::InsertTd(MemoryFragment::FRAGLIST* pFrags,
                          UINT32 Mps,
                          bool IsIsoch,
                          void* pDataBuffer,
                          UINT32 IdtRatio,
                          bool IsSia,
                          UINT16 FrameId)
{
    LOG_ENT();
    Printf(Tee::PriDebug,"(Num Frags = %d, Mps = %d \n)", (UINT32)pFrags->size(), Mps);

    RC rc;

    if ( 0 == Mps )
    {
        Printf(Tee::PriWarn, "Mps = 0, force to 1024 \n");
        Mps = 1024;
    }

    UINT32 numEmptySlot;
    CHECK_RC(GetEmptySlots(&numEmptySlot));
    if ( numEmptySlot < pFrags->size() )
    {
        if ( XHCI_DEBUG_MODE_RING_NO_AUTO_EXPAND & XhciComnDev::GetDebugMode() )
        {   // no auto expanding
            UINT32 totalByte = 0;
            for ( UINT32 i = 0; i < pFrags->size(); i++ )
            {
                totalByte += (*pFrags)[i].ByteSize;
            }
            Printf(Tee::PriError, "%u Bytes in %d fragment, only %d empty TRB left in Transfer Ring, try append more segments \n",
                    totalByte, (UINT32)pFrags->size(), numEmptySlot);
            return RC::BAD_PARAMETER;
        }
        else
        {
            // allocate brand new pages to hold the required normal ring TRBs
            // determeine how many pages we need, note that we need 1 slot as link TRB in each segment
            UINT32 numPages = ( pFrags->size() + (GetTrbPerSegment() - 2) ) / (GetTrbPerSegment() - 1);
            Printf(Tee::PriLow,
                   "  allocating memory for %d TRBs, 64KB segment need: %d \n",
                   (UINT32)pFrags->size(), numPages);
            for ( UINT32 i = 0; i < numPages; i++ )
            {
                CHECK_RC(AppendTrbSeg(0));
            }
        }
    }
    // save buffer structure for later use
    if ( pDataBuffer )
    {
        CHECK_RC(BufferSave(pFrags, pDataBuffer));
    }

    // see also 4.11.2.4 TD Size
    UINT32 tdXferSize = 0;
    for ( UINT32 i = 0; i < pFrags->size(); i++ )
    {
        tdXferSize += (*pFrags)[i].ByteSize;
    }
    UINT32 tdPktCnt = (tdXferSize + Mps -1)/ Mps;
    UINT32 trbXferdSum = 0;
    UINT08 tdSize = 0;

    bool isChain = true;
    bool isIoc = false;
    if ( XHCI_DEBUG_MODE_IOC_ALWAYS_ON & XhciComnDev::GetDebugMode() )
    {
        Printf(Tee::PriLow, "TransferRing::InsertTd() IOC always on\n");
        isIoc = true;
    }
    UINT32 trbInserted = 0;
    for ( UINT32 i = 0; i< pFrags->size(); i++ )
    {
        if ( i == ( pFrags->size()-1 ) )
        {
            isChain = false;
            isIoc = true;
        }
        // setup TD Size
        trbXferdSum += (*pFrags)[i].ByteSize;
        if ( i == (pFrags->size()-1) )
        {   // the last TRB
            tdSize = 0;
        }
        else
        {
            if ( tdPktCnt - (trbXferdSum / Mps) > 31 )
            {  tdSize = 31;  }
            else
            {  tdSize = tdPktCnt - (trbXferdSum / Mps);  }
        }

        bool isIdt = IdtRatio && ((*pFrags)[i].ByteSize <= 8) && (m_EndPoint != 0) && (m_IsDataOut == true) && (DefaultRandom::GetRandom(0, 99) < IdtRatio);
        if ( IsIsoch && (0 == i) )
        {
            CHECK_RC(InsertIsoTrb((*pFrags)[0].Address, (*pFrags)[0].ByteSize, tdSize, IsSia, FrameId, isChain, false, isIdt));
        }
        else
        {
            CHECK_RC(InsertNormalTrb((*pFrags)[i].Address, (*pFrags)[i].ByteSize, tdSize, isIoc, isChain, (0!=i), isIdt));
        }
        trbInserted++;
    }
    // setup the Cycle bit of the first TRB of this TD
    XhciTrb * pTrbPrev;
    CHECK_RC(GetBackwardEntry(trbInserted, &pTrbPrev));
    CHECK_RC(TrbHelper::ToggleCycleBit(pTrbPrev));

    LOG_EXT();
    return OK;
}

// if NumOfTrbsToEn != 0 --> Enable NumOfTrbsToEn TRBs,
//     in this case, the Cycle bit in DeqPtr should be enabled.
// if NumOfTrbsToEn == 0 --> enable all the rest,
//     in this case, the Cycle bit in DeqPtr should be disabled.
RC TransferRing::TruncateTD(UINT32 NumOfTrbsToEn,
                            XhciTrb ** ppTrbToStoppedAt,
                            bool IsAddIocOnly)
{   // HC might cache TRBs, make sure to call me when associated EP is in stopped state!
    // 1. first make sure the tranfer ring is not empty
    // 2. skip forward NumOfTrbsToEn and stop, toggle the TRB's Cycle bit to disabled
    // 3. toggle the Cycle bit of DeqPtr
    RC rc;
    LOG_ENT();

    bool isEmpty;
    CHECK_RC(IsEmpty(&isEmpty));
    if ( isEmpty )
    {
        Printf(Tee::PriWarn, "No pending TRB exists\n");
        return OK;
    }

    XhciTrb *pTrb;
    // locate the DeqPtr and save it
    UINT32 trbSegment, trbIndex;
    CHECK_RC(FindTrb(m_pDeqPtr, &trbSegment, &trbIndex));
    CHECK_RC(GetTrb(trbSegment, trbIndex, &pTrb));

    if ( 0 == NumOfTrbsToEn )
    {   // enable all to the end
        CHECK_RC(TrbHelper::ToggleCycleBit(pTrb));

        if ( ppTrbToStoppedAt )
        {    // setup return value
            CHECK_RC(GetBackwardEntry(1, ppTrbToStoppedAt));
        }
        return OK;
    }
    // NumOfTrbsToEn > 0
    // setup return value
    if ( ppTrbToStoppedAt )
    {
        CHECK_RC(GetForwardEntry(NumOfTrbsToEn - 1, pTrb, ppTrbToStoppedAt));
        // set IOC bit
        CHECK_RC(TrbHelper::SetIocBit(*ppTrbToStoppedAt, true));
        Printf(Tee::PriDebug, "Target hacked. %p \n", *ppTrbToStoppedAt);
        TrbHelper::DumpTrb(*ppTrbToStoppedAt,  Tee::PriDebug);
    }

    // locate the target TRB to disable
    XhciTrb * pTargetTrb;
    CHECK_RC(GetForwardEntry(NumOfTrbsToEn, pTrb, &pTargetTrb));

    if ( pTargetTrb != m_pEnqPtr )
    {   // N pending TRBs left and caller want to skip N-X TRBs
        if ( !IsAddIocOnly )
        {
            CHECK_RC(TrbHelper::ToggleCycleBit(pTargetTrb));
        }
    }

    // CHECK_RC(TrbHelper::ToggleCycleBit(pTrb));

    LOG_EXT();
    return OK;
}

RC TransferRing::InsertNormalTrb(void* pDataBuf,
                                 UINT32 Length,
                                 UINT08 TdSize,
                                 bool IsIoc,
                                 bool IsChain,
                                 bool IsSetupCycleBit,
                                 bool IsIdt)
{
    RC rc;
    LOG_ENT();
    // Classically all the buffers pointed to by a scatter gather list were
    // required to be page size in length except for the first and last.
    // The xHCI does not impose this constraint.
    if ( Length > TrbHelper::GetMaxXferSizeNormalTrb() )
    {
        Printf(Tee::PriError, "Data length %u overflow, valid [0-%u] \n", Length, TrbHelper::GetMaxXferSizeNormalTrb());
        return RC::BAD_PARAMETER;
    }
    if ( !pDataBuf )
    {
        Printf(Tee::PriError, "Buffer Null Pointer \n");
        return RC::BAD_PARAMETER;
    }
    UINT64 pa = (UINT64) Platform::VirtualToPhysical(pDataBuf);
    if (IsIdt)
    {
        Printf(Tee::PriLow, "IDT set\n");
        CHECK_RC(Buf2ImmData((UINT08*)pDataBuf, Length, &pa));
    }
    // get a blank TRB slot
    XhciTrb * pTrb;
    CHECK_RC(InsertEmptyTrb(&pTrb));

    CHECK_RC(TrbHelper::MakeNormalTrb(pTrb, pa, pa>>32, Length, TdSize,
                                      m_IntrTarget, IsIoc, IsIdt, IsChain,
                                      true, false,false, IsSetupCycleBit));

    LOG_EXT();
    return OK;
}

RC TransferRing::InsertSetupTrb(UINT08 ReqType,
                                UINT08 Req,
                                UINT16 Value,
                                UINT16 Index,
                                UINT16 Length)
{
    RC rc;
    LOG_ENT();
    // get a blank TRB slot
    XhciTrb * pTrb;
    CHECK_RC(InsertEmptyTrb(&pTrb));

    CHECK_RC(TrbHelper::MakeSetupTrb(pTrb, ReqType, Req, Value, Index, Length, m_IntrTarget));

    LOG_EXT();
    return OK;
}

RC TransferRing::InsertDataStateTrb(void* pDataBuf,
                                    UINT32 Length,
                                    bool IsDataOut,
                                    bool IsIoc)
{
    RC rc;
    LOG_ENT();
    if ( Length > TrbHelper::GetMaxXferSizeDataStageTrb() )
    {
        Printf(Tee::PriError, "Data length %u overflow, valid [0-%u] \n", Length, TrbHelper::GetMaxXferSizeDataStageTrb());
        return RC::BAD_PARAMETER;
    }
    PHYSADDR pPa = Platform::VirtualToPhysical(pDataBuf);

    // todo: add IDT support
    // get a blank TRB slot
    XhciTrb * pTrb;
    CHECK_RC(InsertEmptyTrb(&pTrb));
    CHECK_RC(TrbHelper::MakeDataStageTrb(pTrb, pPa, pPa>>32, Length, IsDataOut, m_IntrTarget, IsIoc));

    LOG_EXT();
    return OK;
}

RC TransferRing::InsertStastusTrb(bool IsDataOut)
{
    RC rc;
    LOG_ENT();
    // get a blank TRB slot
    XhciTrb * pTrb;
    CHECK_RC(InsertEmptyTrb(&pTrb));

    CHECK_RC(TrbHelper::MakeStatusTrb(pTrb, IsDataOut, m_IntrTarget, m_IsIoc));

    LOG_EXT();
    return OK;
}

RC TransferRing::InsertIsoTrb(void* pDataBuf,
                              UINT32 Length,
                              UINT32 TdSize,
                              bool IsSia,
                              UINT16 FrameId,
                              bool IsChain,
                              bool IsSetupCycleBit,
                              bool IsIdt)
{
    RC rc;
    LOG_ENT();

    if ( Length > TrbHelper::GetMaxXferSizeIsoTrb() )
    {
        Printf(Tee::PriError, "Data length %u overflow, valid [0-%u] \n", Length, TrbHelper::GetMaxXferSizeIsoTrb());
        return RC::BAD_PARAMETER;
    }
    UINT64 pa = (UINT64) Platform::VirtualToPhysical(pDataBuf);
    if (IsIdt)
    {
        CHECK_RC(Buf2ImmData((UINT08*)pDataBuf, Length, &pa));
    }

    // get a blank TRB slot
    XhciTrb * pTrb;
    CHECK_RC(InsertEmptyTrb(&pTrb));
    CHECK_RC(TrbHelper::MakeIsoTrb(pTrb, pa, pa>>32, Length, TdSize,
                                   IsSia, FrameId, m_IntrTarget, m_IsIoc, IsIdt, IsChain,
                                   true, false, false,IsSetupCycleBit));

    LOG_EXT();
    return OK;
}

RC TransferRing::InsertEventDataTrb(bool IsChain,
                                    bool IsEvalNext,
                                    bool IsHandlePrev)
{
    RC rc;
    LOG_ENT();

    // get a blank TRB slot
    XhciTrb * pTrb;
    CHECK_RC(InsertEmptyTrb(&pTrb));
    PHYSADDR pPa = Platform::VirtualToPhysical(pTrb);

    CHECK_RC(TrbHelper::SetupEventDataTrb(pTrb, pPa, pPa>>32, m_IntrTarget, IsChain, IsEvalNext));

    // deal with preceeding TRB
    if ( IsHandlePrev )
    {
        XhciTrb * pTrbPrev;
        CHECK_RC(GetBackwardEntry(2, &pTrbPrev));    // use 2 here to point to previous TRB
        // turn off the IOC bit
        CHECK_RC(TrbHelper::SetIocBit(pTrbPrev, false));
        // turn on the Chain bit
        CHECK_RC(TrbHelper::SetChainBit(pTrbPrev, true));
        UINT08 type;
        CHECK_RC(TrbHelper::GetTrbType(pTrbPrev, &type));
        if ( (XhciTrbTypeNormal == type)
             || (XhciTrbTypeData == type)
             || (XhciTrbTypeIsoch == type) )
        {   // turn off the ISP bit
            CHECK_RC(TrbHelper::SetIspBit(pTrbPrev, false));
        }
    }

    LOG_EXT();
    return OK;
}

RC TransferRing::InsertNoOpTrb()
{
    RC rc;
    LOG_ENT();
    // get a blank TRB slot
    XhciTrb * pTrb;
    CHECK_RC(InsertEmptyTrb(&pTrb));

    CHECK_RC(TrbHelper::MakeNoOpTrb(pTrb, m_IntrTarget));

    LOG_EXT();
    return OK;
}

