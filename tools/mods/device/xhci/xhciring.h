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

// This is the header file of all kinds of Rings i.e.
// Command Ring, Event Ring & Transfer Ring

#ifndef INCLUDED_XHCIRING_H
#define INCLUDED_XHCIRING_H

#ifndef INCLUDED_XHCITRB_H
    #include "xhcitrb.h"
#endif
#ifndef INCLUDED_MEMFRAG_H
    #include "core/include/memfrag.h"
#endif

#define XHCI_RING_ERST_ENTRY_SIZE   16
#define XHCI_ALIGNMENT_ERST         64
#define XHCI_ALIGNMENT_CMD_RING     64
#define XHCI_ALIGNMENT_XFER_RING    16
#define XHCI_ALIGNMENT_EVENT_RING   64

#define XHCI_RING_TYPE_NA           0
#define XHCI_RING_TYPE_XFER         1
#define XHCI_RING_TYPE_CMD          2
#define XHCI_RING_TYPE_EVENT        3

#define XHCI_ERST_DW0_RSVD          3:0
#define XHCI_ERST_DW0_BASE_LO       31:4
#define XHCI_ERST_DW1_BASE_HI       31:0
#define XHCI_ERST_DW2_SIZE          15:0
#define XHCI_ERST_DW3_RSVD          31:0

class XhciController;
class XhciComnDev;
class InputContext;

class XhciTrbRing
{
public:
    XhciTrbRing();
    virtual ~ XhciTrbRing();

    //! \brief Get the base address of first segment
    //--------------------------------------------------------------------------
    RC GetBaseAddr(PHYSADDR * ppPhyBase);

    //! \brief Get TRB backwardly from current Enq Pointrt
    // applies to Xfer & Cmd Rings
    //! \sa GetForwardEntry()
    //--------------------------------------------------------------------------
    RC GetBackwardEntry(UINT32 BackLevel, XhciTrb ** ppTrb,
                        UINT32 *pSeg = NULL, UINT32 *pIndex = NULL);

    //! \brief Get TRB forewardly from given location (pNeedle)
    // applies to Xfer & Cmd Rings. if pSeg & pIndex were set, ppTrb is ignored
    //! \sa GetBackwardEntry()
    //--------------------------------------------------------------------------
    RC GetForwardEntry(UINT32 FowardLevel, XhciTrb * pNeedle, XhciTrb ** ppTrb,
                       UINT32 *pSeg = NULL, UINT32 *pIndex = NULL);

    //! \brief Callwlate the number of empty slots left in this Ring
    // applies to Xfer & Cmd Rings.
    //--------------------------------------------------------------------------
    RC GetEmptySlots(UINT32 *pNumSlots);

    //! \brief Update Deq Ptr for CMD/Xfer Rings according to just finished TRB
    // applies to Xfer & Cmd Rings.
    //--------------------------------------------------------------------------
    RC UpdateDeqPtr(XhciTrb* pFinishedTrb, bool IsAdvancePtr = true);

    //! \brief Full condition detection
    // "Advance the Enq will make it equal to Deq for Transfer and Cmd Ring"
    //--------------------------------------------------------------------------
    RC IsFull(bool * pIsFull);
    // "Enq = Deq"
    RC IsEmpty(bool * pIsEmpty);

    //! \brief Determine if a TRB is owned by SW
    // if (TRB - Deq) >= (Enq - Deq)
    //--------------------------------------------------------------------------
    RC IsOwnBySw(XhciTrb *pTrb, bool *pIsBySw);

    //! \brief Get Physical Address of ENqPtr and Pcs
    //--------------------------------------------------------------------------
    RC GetEnqInfo(PHYSADDR * ppPa, bool * IsPcs, XhciTrb **ppEnqPtr = NULL);

    //! \brief Info functions
    //--------------------------------------------------------------------------
    virtual RC PrintInfo(Tee::Priority Pri) = 0;
    virtual RC PrintTitle(Tee::Priority Pri) = 0;

    //! \brief Identify helper functions
    //--------------------------------------------------------------------------
    bool IsXferRing();
    bool IsCmdRing();
    bool IsEventRing();

    //! \brief Retrieve/Setup TRB located at specified location
    //--------------------------------------------------------------------------
    RC GetTrbEntry(UINT32 SegIndex, UINT32 TrbIndex, XhciTrb * pTrb);
    RC SetTrbEntry(UINT32 SegIndex, UINT32 TrbIndex, XhciTrb * pTrb);

    //! \brief Get current Enq/Deq location
    // for Xfer/Cmd Ring, it's EnqPtr, for Event Ring, it's DeqPtr
    //--------------------------------------------------------------------------
    RC GetPtr(UINT32 * pSegment, UINT32 * pIndex);

    //! \brief Get vector comprised of Segment sizes
    //--------------------------------------------------------------------------
    RC GetSegmentSizes(vector<UINT32> *pvSizes);

    inline UINT32 Abs(int arg)
    {
       return (UINT32)((arg<0)?(-arg):arg);
    }

    //! \brief Find the location of specified TRB
    //--------------------------------------------------------------------------
    RC FindTrb(XhciTrb* pTrb, UINT32 * pSeg, UINT32 * pIndex, bool IsSilent = false);

    //! \brief Clear the Ring to its initial state (all 0)
    //! This is for debug purpose, use along with PrintRing() from JS
    //! when ClearRing() is called, PrintRing() would give more succinct output
    //! note: return error if ring is not empty, link TRBs and Cycly Bits will remain un-changed
    //--------------------------------------------------------------------------
    RC Clear();

    //! \brief Name exaplains itself
    //--------------------------------------------------------------------------
    static UINT32 GetTrbPerSegment();

protected:
    //! \brief For use by SetRingPtr() provided in JS interface
    // Backdoor to manually set Enq or Deq
    //--------------------------------------------------------------------------
    RC SetPtr(UINT32 SegIndex, UINT32 TrbIndex, bool IsEnqPtr);

    //! \brief Find number of TRBs(include possible Link TRBs) between two TRBs
    //--------------------------------------------------------------------------
    RC FindGap(XhciTrb* pTrbStart, XhciTrb* pTrbEnd, UINT32 * pNumSlots);

    //! \brief Append a segment having given number of initializd TRBs (all 0)
    // Link TRB ilwolved will also be updated
    // Cycble bits update will be done by child class override functions
    //--------------------------------------------------------------------------
    RC AppendTrbSeg(UINT32 NumElem,
                    INT32 PrevSegIndex,
                    bool IsHandleLink,
                    UINT32 AddrBit = 32);
    //! \brief Remove a segment
    // Link TRB ilwolved will also be updated
    //--------------------------------------------------------------------------
    RC RemoveTrbSeg(UINT32 SegIndex, bool IsHandleLink);

    // very intersting, one Ring could have two link TRBGs which Toggle Cycle bit is 1
    //! \brief Get pointer of TRB at specified location
    //--------------------------------------------------------------------------
    RC GetTrb(UINT32 Seg, INT32 Index, XhciTrb ** ppTrb);

    //! \brief Init (clear to 0 except for PCS) a TRB at current Enq Ptr and return the address.
    // Advance the Enq Ptr, handle the Cycle bit of Link TRB accrossed if applicable
    // Toggle m_CycleBit if encounter set TC
    //--------------------------------------------------------------------------
    RC InsertEmptyTrb(XhciTrb ** ppTrb);

    RC VerifyPointer();        //for debug purpose, to be remove after code stable

    //! \brief The type of 'this'
    //! This should only be modified in constructor
    //--------------------------------------------------------------------------
    UINT08      m_RingType;        //Transfer, Command, Event

    XhciTrb *   m_pEnqPtr;
    XhciTrb *   m_pDeqPtr;

    //! \brief PCS for Xfer & CMD ring, CCS for Event ring
    //--------------------------------------------------------------------------
    bool        m_CycleBit;

    //! \brief Alignment requirement, defferent Ring has different alignment
    //! This should only be modified in constructor
    //--------------------------------------------------------------------------
    UINT32 m_RingAlignment;

    //! \brief Segments list
    //--------------------------------------------------------------------------
    MemoryFragment::FRAGLIST    m_TrbSeglist;

    //! \brief Location of Ptr, should be corresponding to m_pEnqPtr or m_pDeqPtr
    // SA m_TrbSeglist.
    //--------------------------------------------------------------------------
    UINT32 m_LwrrentSegment;
    UINT32 m_LwrrentIndex;

    // this it or debug purpose, to be removed
    UINT32 m_PrintSegment;
    UINT32 m_PrintIndex;
};

//------------------------------------------------------------------------------
class CmdRing:public XhciTrbRing
{
public:
    CmdRing(XhciController * pHost); // create and append
    virtual ~ CmdRing();
    RC Init(UINT32 NumElem, UINT32 AddrBit = 32);

    // See also XhciTrbRing::SetPtr()
    RC SetEnqPtr(UINT32 SegIndex, UINT32 TrbIndex, INT32 CycleBit = 0);
    RC GetPcs(bool * pIsCBit);

    // to report the current value of the Command Ring Dequeue Pointer.
    // Note that: ppEeqPointer returns the Enqueue pointer of Cmd Ring
    RC IssueNoOp(XhciTrb ** ppEeqPointer = NULL, bool IsIssue = true, UINT32 WaitMs = 2000);
    // to obtain an available Device Slot and to transition a Device Slot from
    // the Disabled to the Default state.
    RC IssueEnableSlot(UINT08 * pSlotId,
                       UINT08 SlotType = 0,
                       bool IsIssue = true,
                       UINT32 WaitMs = 2000);
    // to free a Device Slot when a USB device is disconnected, SW can free the associated data structures
    RC IssueDisableSlot(UINT08 SlotId, bool IsIssue = true, UINT32 WaitMs = 2000);
    // to transition a Device Slot from the Default to the Addressed state.
    // then SW could issue Get_Configuration to device
    RC IssueAddressDev(UINT08 SlotId, InputContext * pInpCntxt, bool IsIssue = true, UINT32 WaitMs = 2000, bool IsBsr = false);
    // to enable, disable, or reconfigure endpoints associated with a target configuration.
    // this should be issue prior to issuing USB SET_CONFIGURATION request
    RC IssueCfgEndp(InputContext * pInpCntxt, UINT08 SlotId, bool IsDeCfg = false, bool IsIssue = true, UINT32 WaitMs = 2000);
    // to inform the xHC that specific fields in the Device Context data structures have been modified.
    RC IssueEvalContext(InputContext * pInpCntxt, UINT08 SlotId, bool IsIssue = true, UINT32 WaitMs = 2000);
    // to recover from a halted condition on an endpoint to enable doorbell etc.
    RC IssueResetEndp(UINT08 SlotId, UINT08 EndPoint, bool IsTransferStatePreserve = true, bool IsIssue = true, UINT32 WaitMs = 2000);
    // to stop the xHC exelwtion of the TDs on an endp
    RC IssueStopEndp(UINT08 SlotId, UINT08 Dci, bool IsSp, bool IsIssue = true, UINT32 WaitMs = 2000);
    // to modify the TR Dequeue Pointer field of an Endpoint or Stream Context.
    RC IssueSetDeqPtr(UINT08 SlotId, UINT08 Dci, PHYSADDR pPaDeq, bool IsDcs, UINT08 StreamId = 0, UINT08 Sct = 0, bool IsIssue = true, UINT32 WaitMs = 2000);
    // to inform the xHC that the USB Device associated with a Device Slot has been Reset.
    RC IssueResetDev(UINT08 SlotId, bool IsIssue = true, UINT32 WaitMs = 2000);
    // for VMM
    RC IssueForceEvent();
    // to initiate Bandwidth Request Events for Isoch endpoints
    RC IssueNegoBandwidth(UINT08 SlotId);
    RC IssueSetLatencyTol();
    // for SW to retrieve the percentage of Total Available Bandwidth on each Root Hub Port
    RC IssueGetPortBandwidth(void * pPortBwContext, UINT08 DevSpeed, UINT08 HubSlotId, bool IsIssue = true, UINT32 WaitMs = 2000);
    // to send a Link Management (LMP) or Transaction Packet (TP) to a USB device,
    // through a selected Root Hub Port. For instance, it may be used to send a PING TP or a Vendor Device Test LMP.
    RC IssueForceHeader(UINT32 Lo, UINT32 Mid, UINT32 Hi, UINT08 PacketType, UINT08 PortNum, bool IsIssue = true, UINT32 WaitMs = 2000);
    RC IssueGetExtndProperty(UINT08 SlotId,
                             UINT08 EpId,
                             UINT08 SubType,
                             UINT16 Eci,
                             void * pExtndPropContext = nullptr,
                             bool IsIssue = true,
                             UINT32 WaitMs = 2000);

    RC AppendTrbSeg(UINT32 NumElem, INT32 PrevSegIndex = -1, UINT32 AddrBit = 32);
    RC RemoveTrbSeg(UINT32 SegIndex);

    virtual RC PrintInfo(Tee::Priority Pri);
    virtual RC PrintTitle(Tee::Priority Pri);
private:

    XhciController * m_pHost;
};

//-----------------------------------------------------------------------------
class TransferRing:public XhciTrbRing
{
public:
    TransferRing(UINT08 SlotId,
                 UINT08 Endpoint,
                 bool IsOut,
                 UINT16 StreamId = 0,
                 bool IsEp0HasDir = false);
    virtual ~ TransferRing();
    RC Init(UINT32 NumElem, UINT32 AddrBit = 32);

    RC SetEnqPtr(UINT32 SegIndex, UINT32 TrbIndex, INT32 CycleBit = 0);
    RC GetPcs(bool * pIsCBit);

    RC SetInterruptTarget(UINT32 IntrTarget);
    RC GetInterruptTarget(UINT32* pIntrTarget);

    RC SetIoc(bool IsIoc);

    RC InsertTd(MemoryFragment::FRAGLIST* pFrags,
                UINT32 Mps,
                bool IsIsoch,
                void* pDataBuffer = NULL,
                UINT32 IdtRatio = 0,
                // below are for Isoch TD
                bool IsSia = true,
                UINT16 FrameId = 0);

    RC TruncateTD(UINT32 NumOfTrbsToEn,
                  XhciTrb ** ppTrbToStoppedAt = NULL,
                  bool IsAddIocOnly = false);

    // Insert Normal TRB at current location
    // See also Spec Figure 6 for Page accrossing handling
    RC InsertNormalTrb(void* pDataBuf,
                       UINT32 Length,
                       UINT08 TdSize,
                       bool IsIoc = true,
                       bool IsChain = false,
                       bool IsSetupCycleBit = true,
                       bool IsIdt = false);

    RC InsertSetupTrb(UINT08 ReqType, UINT08 Req, UINT16 Value, UINT16 Index, UINT16 Length);

    RC InsertDataStateTrb(void* pDataBuf, UINT32 Length, bool IsDataOut, bool IsIoc = false);
    RC InsertStastusTrb(bool IsDataOut);

    RC InsertIsoTrb(void* pDataBuf,
                    UINT32 Length,
                    UINT32 TdSize,
                    bool IsSia = true,
                    UINT16 FrameId = 0,
                    bool IsChain = false,
                    bool IsSetupCycleBit = true,
                    bool IsIdt = false);
    RC InsertEventDataTrb(bool IsChain = false, bool IsEvalNext = false, bool IsHandlePrev = true);
    RC InsertNoOpTrb();

    RC AppendTrbSeg(UINT32 NumElem, UINT32 AddrBit = 32);
    RC RemoveTrbSeg(UINT32 SegIndex);

    bool IsThatMe(UINT08 SlotId, UINT08 Endpoint, bool IsDataOut = true, UINT16 StreamId = 0, bool IsSkipStreamID = false);
    bool IsThatMe(UINT08 SlotId);

    RC GetHost(UINT08 * pSlotId, UINT08 * pEndpoint, bool * pIsDataOut, UINT16 * pStreamId = NULL);

    virtual RC PrintInfo(Tee::Priority Pri);
    virtual RC PrintTitle(Tee::Priority Pri);

    // the whole process is like:
    // User indicate a Memory late release by given a pointer to the buffer
    // Ring Save the pointer and Fraglist
    // User retrieve the fraglist & then the data
    // User release the buffer explicitly
    RC BufferSave(MemoryFragment::FRAGLIST* pFrags, void * pBuffer);
    RC BufferRetrieve(MemoryFragment::FRAGLIST * pFrags, UINT32 NumBuf = 0);
    RC BufferRelease(UINT32 NumBuf = 0);

private:
    RC Buf2ImmData(UINT08 * pData, UINT32 Length, UINT64 *pImmData);

    // the EP address this ring attached to (as opposite to DeqPtr of a Endpoint)
    UINT08  m_SlotId;
    UINT08  m_EndPoint;
    bool    m_IsDataOut;
    bool    m_IsEp0HasDir;  //Endpoint 0 has two directions, used by Device Mode

    UINT16  m_StreamId;

    // the default Interrupt target
    UINT32  m_IntrTarget;
    // the default Interrupt on Completion state
    bool    m_IsIoc;

    // MODS provides each Transfer Ring a place to hold the pointers later release buffers
    // each item in m_FragList represents a TRB in the Ring
    MemoryFragment::FRAGLIST m_FragList;
    // if this is not empty, client wants Xfer Ring to do later release
    // note that the Address field of any element is the virtual address of TD buffer
    // since one TD could contain multiple TRBs, the ByteSize is used to store
    // the number of TRBs of current TD.
    // thus any entry in m_vpBuffer could be mapped to one or more entries in m_FragList
    MemoryFragment::FRAGLIST m_vpBuffer;
};

//-----------------------------------------------------------------------------
// Event Ring, Event Segment Table related
//-----------------------------------------------------------------------------
class EventRing:public XhciTrbRing
{
public:
    EventRing(XhciComnDev * pHost, UINT32 Index, UINT32 ErstSize);
    virtual ~ EventRing();
    RC Init(UINT32 NumElem, UINT32 AddrBit = 32);

    RC SetDeqPtr(UINT32 SegIndex, UINT32 TrbIndex, INT32 CycleBit);
    // to set HW registers
    RC GetERSTBA(PHYSADDR * ppERSTBA, UINT32 * pSize);
    // to set HW registers for Device Mode
    RC GetSegmentBase(UINT32 SegIndex, PHYSADDR * ppSegment);

    // Wait for certain type of event
    RC WaitForEvent(UINT08 EventType,
                    UINT32 WaitMs = 2000,
                    XhciTrb *pEventTrb = NULL,
                    bool IsFlushEvents = false,
                    UINT08 SlotId = 0,             // if not 0, wait for Event generated by specific Xfer Ring
                    UINT08 Dci = 0);
    // Wait for an Transfer Complete Event TRB / Command Completion Event TRB
    // for an specicic Normal / Command TRB to show up in the event ring
    RC WaitForEvent(XhciTrb * pTrb,
                    UINT32 WaitMs = 2000,
                    XhciTrb *pEventTrb = NULL,
                    bool IsFlushEvents = false,
                    bool IsExitOnError = false);

    typedef struct _EventRingSegTblEntry
    {
        UINT32 DW0;
        UINT32 DW1;
        UINT32 DW2;
        UINT32 DW3;
    } EventRingSegTblEntry;

    RC AppendTrbSeg(UINT32 NumElem, UINT32 AddrBit = 32);
    RC RemoveTrbSeg();

    UINT32 GetNumEvent() { return (UINT32) m_vEventQueue.size(); }
    // flush all the event cached in Event Ring and local queue
    RC FlushRing(UINT32 * pNumEvent = NULL);
    bool CheckXferEvent(UINT08 SlotId, UINT08 Dci, XhciTrb *pTrb);

    bool GetNewEvent(XhciTrb *pTrb, UINT32 TimeoutMs);

    virtual RC PrintInfo(Tee::Priority Pri);
    virtual RC PrintTitle(Tee::Priority Pri);
    RC PrintERST(Tee::Priority Pri);

protected:
    //! \brief Collect Event TRBs from Event Ring to Local Event Queue
    //! This is one of the core functions of this class, SA WaitForEvent()
    //--------------------------------------------------------------------------
    RC GetEvent(UINT32 TimeoutMs, UINT32* pNumEvent = NULL);

    //! \brief Determine the path of where HW put new Event TRB
    //! In certain cases of Segment adding / deleting. SW need to find out if
    // HW has start to use the newly added segment / or stop to use the deleted
    // segment. This function it to handle such situation
    //--------------------------------------------------------------------------
    RC SolveEnqRoute(INT32 PendingSegment, bool * pIsSolved);

    //! \brief Write m_pDeqPtr, m_LwrrentSegment into HW registers
    //--------------------------------------------------------------------------
    // RC UpdateRegisterDeqPtr();

private:

    RC WaitForEventPortal(UINT08 EventType,
                          void * Ptr,
                          UINT32 WaitMs,
                          XhciTrb *pTrb,
                          bool IsFlushEvents,
// if search by TRB address following two params will be skipped.
                          UINT08 SlotId = 0,
                          UINT08 Dci = 0,
// following only apply to search by address
                          bool IsExitOnError = false);

    // MAX size of Event Ring Segment Table
    UINT32 m_ErstSize;

    // identify of Event Ring itself, Primary or Secondary interrupt
    UINT32 m_Index;

    XhciComnDev * m_pHost;

    // will be returned by GetERSTBA()
    EventRingSegTblEntry * m_pEventRingSegTbl;

    // indicate that Deq Pointer might get into a un-decided state. (there's a segment pending)
    // ABS(m_DeqPending) is the pending segment number
    // 0 - normal state, >0 - new Segment added to ERST, < 0 - Segment deleted from ERST
    // set when any update happens to ERSTSZ
    // clear when Enq Pointer track has been found out.
    //   for segment adding, this means HW start to use the first TRB of  pending segment.
    //      action required: set m_IsUsingCompletionCode
    //   for segment removing, this means HW has jumped over the to-be-del segment
    //      action required: delete the to-be-del segment & release memory
    INT32 m_DeqPending;

    // indicate there's a segment to be deleted
    // this should happed as soon as we decide HW has stopped to use the to-be-deleted segment
    // 0 - no Segment to delete
    // set when we m_DeqPendin changes from minus -> 0 but we can't delete the to-be-del segment yet.(in use)
    // clear when m_SegToDel != 0 and deleted the to-be-del segment
    // UINT32 m_SegToDel;

    // if to use Completion Code inplace of Cycle bit as the Enq position indicator, by defualt use Cycle bit
    // set when new segmeng was added and EnqPtr has been resolved to use this new segment
    // clear when segment advanced and m_IsUsingCompletionCode is true.
    bool m_IsUsingCompletionCode;

    // indicate the the TRB pointed by m_pDeqPtr has been processed & obslete.
    // we must resolve the path of EnqPtr before retieve any Event TRB.
    // set when GetEvent() get stuck on advance DeqPtr
    // clear when the path of EnqPtr has ben found out and DeeqPtr has been advanced
    bool m_IsResolveNeeded;

    // Local Event Queue, when event TRBs are collect into queue, Deq pointer advanced
    // Since segment could be appended or removed, we use local data copy instead of address refference
    vector<XhciTrb> m_vEventQueue;
};

#endif
