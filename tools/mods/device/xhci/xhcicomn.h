/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_XHCICOMN_H
#define INCLUDED_XHCICOMN_H

#ifndef INCLUDED_CONTROLLER_H
#include "cheetah/include/controller.h"
#endif

#ifndef INCLUDED_RC_H
    #include "core/include/rc.h"
#endif

#ifndef INCLUDED_XHCITRB_H
    #include "xhcitrb.h"
#endif

#ifndef INCLUDED_TASKER_H
    #include "core/include/tasker.h"
#endif

#ifndef INCLUDED_XHCIRING_H
    #include "xhciring.h"
#endif

#define XHCI_DEBUG_MODE_PRINT_MORE          (1<<0)
#define XHCI_DEBUG_MODE_LOAD_FW_ONLY        (1<<1)
#define XHCI_DEBUG_MODE_WRITE_FW            (1<<2)
#define XHCI_DEBUG_MODE_NO_WAIT_CNR         (1<<3)
#define XHCI_DEBUG_MODE_FAST_TRANS          (1<<4)
#define XHCI_DEBUG_MODE_RING_NO_AUTO_EXPAND (1<<5)
#define XHCI_DEBUG_MODE_USE_SCSI_CMD16      (1<<6)
#define XHCI_DEBUG_MODE_SKIP_MAILBOX        (1<<7)
#define XHCI_DEBUG_MODE_IOC_ALWAYS_ON       (1<<8)

#define XUSB_DEBUG_MODE_PRINT_PRD_INFO      (1<<1)
#define XUSB_DEBUG_MODE_EN_UNDERRUN         (1<<2)

#define XHCI_DEFAULT_TIMEOUT_MS                 2000
#define XHCI_DEFAULT_TIMEOUT_MS_MAGIC_WORD      0xdeadbeef

// this is the base class of both USB3 host and device mode controllers.
// it contains the common interfaces for host and device mode
class XhciComnDev:public Controller
{
public:
    //==========================================================================
    // Natives
    //==========================================================================
    XhciComnDev();
    virtual ~ XhciComnDev();

    //! \brief Retrieve Tasker::EventId associated with specified Event Ring
    //! \param [in] RingIndex : Index of Event Ring
    //! \param [out] pEventId : pointer of Event ID
    virtual RC GetEventId(UINT32 RingIndex, Tasker::EventID * pEventId) {*pEventId = NULL; return OK;}

    //! \brief handler of the append/shrink of Event Ring segments,
    //!  mainly used to update HC registers
    //! \param [in] EventRingIndex : Index of changing Event Ring
    //! \param [in] NumSegments : new number of segments
    virtual RC EventRingSegmengSizeHandler(UINT32 EventRingIndex, UINT32 NumSegments) {return OK;}

    //! \brief Handle the newly arrived Event TRB
    //! \param [in] pTrb : pointer to the newly arrived Event TRB
    virtual RC NewEventTrbHandler(XhciTrb* pTrb) = 0;

    //! \brief Update the Event Ring Deq Pointer
    //!  mainly used to update HC registers
    //! \param [in] EventIndex : Index of Event Ring
    //! \param [in] pDeqPtr : new Event Ring Dequeue Pointer
    //! \param [in] SegmengIndex : Current segment of the Event Ring
    virtual RC UpdateEventRingDeq(UINT32 EventIndex, XhciTrb * pDeqPtr, UINT32 SegmengIndex) = 0;

    //! \brief Getter of m_DebugMode
    static UINT32 GetDebugMode();
    //! \brief Setter of m_DebugMode
    static RC SetDebugMode(UINT32 DebugMode, bool IsForceValue = false);

    //! \brief Update Xfer Ring Dequeue Pointer
    //! \param [in] pvpAllXferRings : vector of all Transfer Rings in system
    //! \param [in] SlotId : Slot ID to which the Xfer Ring belongs
    //! \param [in] Endpoint : Ep number  to which the Xfer Ring belongs
    //! \param [in] IsDataOut :  Data Dir. to which the Xfer Ring belongs
    //! \param [in] phyTrb : the latest physical address of DeqPtr
    RC UpdateXferRingDeq(vector<TransferRing *>  *pvpAllXferRings,
                         UINT32 SlotId,
                         UINT08 Endpoint,
                         bool IsDataOut,
                         PHYSADDR phyTrb);

    //! \brief Retreive the timeout value user specified
    //! \param [out] pTimeoutMs :
    //! if not NULL and user has called SetTimeout, the user specified timeout value will returned in it
    //!    if user hasn't specified the timeout, data referred by pTimeoutMs will not be changed
    //! if NULL, return the current timeout data
    static UINT32 GetTimeout(UINT32 * pTimeoutMs = NULL);
    //! \brief Specify the timeout value
    //! \param [in] TimeoutMs : timeout in Ms, see also GetTimeout()
    //~ if TimeoutMs == 0, clear user setting and use MODS default
    static RC SetTimeout(UINT32 TimeoutMs);

    //RC WaitMem(void *pVirtMem, UINT32 Exp, UINT32 UnExp, UINT32 TimeoutMs);
    //RC WaitBitSetAnyMem(void *pVirtMem, UINT32 Exp, UINT32 TimeoutMs);
protected:
    //! \var Debug Mode bit mask, bit definitions could be found at the beginning of this file
    static UINT32 m_DebugMode;

    //! \var  default timeout MS for waiting
    static UINT32 m_TimeoutMs;

};

#endif // INCLUDED_XHCICOMN_H
