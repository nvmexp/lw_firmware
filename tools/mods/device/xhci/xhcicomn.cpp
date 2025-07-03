/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "xhcicomn.h"
#include "cheetah/include/logmacros.h"
#include "cheetah/include/tegradrf.h"
#include "core/include/platform.h"
#include "device/include/memtrk.h"
#include "core/include/tasker.h"

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME "XhciComnDev"

UINT32 XhciComnDev::m_DebugMode = 0;
UINT32 XhciComnDev::m_TimeoutMs = XHCI_DEFAULT_TIMEOUT_MS_MAGIC_WORD;

XhciComnDev::XhciComnDev()
: Controller(true)
{
    LOG_ENT();
    LOG_EXT();
}

XhciComnDev::~XhciComnDev()
{
    LOG_ENT();
    LOG_EXT();
}

UINT32 XhciComnDev::GetDebugMode()
{
    return m_DebugMode;
}

RC XhciComnDev::SetDebugMode(UINT32 DebugMode, bool IsForceValue)
{
    if ( IsForceValue )
    {
        m_DebugMode = DebugMode;
    }
    else
    {
        m_DebugMode ^= DebugMode;
    }

    Printf(Tee::PriNormal, "Debug Mode 0x%x \n", m_DebugMode);
    Printf(Tee::PriLow, "\nFor Host Mode - \n");
    Printf(Tee::PriLow, "PrintMore: %s, LoadFwOnly: %s, WriteFwOnly %s, NoWaitCNR %s \n",
           (XHCI_DEBUG_MODE_PRINT_MORE & m_DebugMode) ? "T":"F",
           (XHCI_DEBUG_MODE_LOAD_FW_ONLY & m_DebugMode) ? "T":"F",
           (XHCI_DEBUG_MODE_WRITE_FW & m_DebugMode) ? "T":"F",
           (XHCI_DEBUG_MODE_NO_WAIT_CNR & m_DebugMode) ? "T":"F");
    Printf(Tee::PriLow, "  FastEnum: %s, NoRingAutoExp: %s, UseCmd16 %s, SkipMailbox %s, IocAlwaysOn %s \n",
           (XHCI_DEBUG_MODE_FAST_TRANS & m_DebugMode) ? "T":"F",
           (XHCI_DEBUG_MODE_RING_NO_AUTO_EXPAND & m_DebugMode) ? "T":"F",
           (XHCI_DEBUG_MODE_USE_SCSI_CMD16 & m_DebugMode) ? "T":"F",
           (XHCI_DEBUG_MODE_SKIP_MAILBOX & m_DebugMode) ? "T":"F",
           (XHCI_DEBUG_MODE_IOC_ALWAYS_ON & m_DebugMode) ? "T":"F");

    Printf(Tee::PriLow, "\nFor Device Mode - \n");
    Printf(Tee::PriLow, "PrintMore: %s, PrintPrd: %s, ReadBufUnderRun: %s \n",
           (XHCI_DEBUG_MODE_PRINT_MORE & m_DebugMode) ? "T":"F",
           (XUSB_DEBUG_MODE_PRINT_PRD_INFO & m_DebugMode) ? "T":"F",
           (XUSB_DEBUG_MODE_EN_UNDERRUN & m_DebugMode) ? "T":"F");
    return OK;
}

RC XhciComnDev::UpdateXferRingDeq(vector<TransferRing *>  *pvpAllXferRings,
                                  UINT32 SlotId,
                                  UINT08 Endpoint,
                                  bool IsDataOut,
                                  PHYSADDR phyTrb)
{
    LOG_ENT();
    if ( !phyTrb )
    {
        Printf(Tee::PriError, "Xfer TRB null pointer");
        return RC::BAD_PARAMETER;
    }
    RC rc;
    XhciTrb * pTrb = (XhciTrb *)MemoryTracker::PhysicalToVirtual(phyTrb);
    if ( !pTrb )
    {
        Printf(Tee::PriWarn, "Physic Address 0x%llx seems invalid, skip this Event TRB \n  You can dump the EventRing to see more detail of this error \n ", phyTrb);
        return OK;
    }

    vector<TransferRing *> vpXferRings;
    for ( UINT32 i = 0; i<pvpAllXferRings->size(); i++ )
    {
        if ( NULL == (*pvpAllXferRings)[i] )
        {
            Printf(Tee::PriError, "Transfer Ring NULL pointer");
            return RC::SOFTWARE_ERROR;
        }
        if ( (*pvpAllXferRings)[i]->IsThatMe(SlotId, Endpoint, IsDataOut, 0, true) )
        {   // save it if matches
            vpXferRings.push_back((*pvpAllXferRings)[i]);
        }
    }
    if ( 0 == vpXferRings.size() )
    {
        Printf(Tee::PriError, "Xfer Ring not found for Slot %d, Endpoint %d, Data %s", SlotId, Endpoint, IsDataOut?"Out":"In");
        return RC::BAD_PARAMETER;
    }

    if ( 1 == vpXferRings.size() )
    {   // find only one ring
        CHECK_RC(vpXferRings[0]->UpdateDeqPtr(pTrb));
    }
    else
    {
        // more than one Xfer Ring associated, need to screen them
        Printf(Tee::PriLow, "%d Xfer Rings associated with EP %d, Data %s\n", (UINT32)vpXferRings.size(), Endpoint, IsDataOut?"Out":"In");
        UINT32 dummy0, dummy1;
        UINT32 ringIndex = 0;
        for ( ringIndex = 0; ringIndex < vpXferRings.size(); ringIndex++ )
        {
            Printf(Tee::PriLow, "  Searching TRB 0x%llx in ", phyTrb);
            vpXferRings[ringIndex]->PrintTitle(Tee::PriLow);
            if (OK == vpXferRings[ringIndex]->FindTrb(pTrb, &dummy0, &dummy1, true))
                break;
        }
        if ( vpXferRings.size() == ringIndex )
        {
            Printf(Tee::PriError, "TRB 0x%llx not found on any Transfer Ring of Stream @ Slot %d, Endpoint %d, Data %s",
                    phyTrb, SlotId, Endpoint, IsDataOut?"Out":"In");
            return RC::BAD_PARAMETER;
        }
        CHECK_RC(vpXferRings[ringIndex]->UpdateDeqPtr(pTrb));
    }
    return OK;
}

UINT32 XhciComnDev::GetTimeout(UINT32 * pTimeoutMs)
{
    if (pTimeoutMs)
    {   // caller wants to adopt user specified timeout if it exists, otherwise don't touch it
        if (XHCI_DEFAULT_TIMEOUT_MS_MAGIC_WORD != m_TimeoutMs)
        {
            * pTimeoutMs = m_TimeoutMs;
           // in this case, the return value a dummy
        }
    }
    else
    {
        if (XHCI_DEFAULT_TIMEOUT_MS_MAGIC_WORD == m_TimeoutMs)
        {   // if user hasn't toouch the timeut value, return default
            return XHCI_DEFAULT_TIMEOUT_MS;
        }
    }
    return m_TimeoutMs;
}

RC XhciComnDev::SetTimeout(UINT32 TimeoutMs)
{
    if ( TimeoutMs )
    {
        Printf(Tee::PriNormal, "Set default timeout to %dms \n", TimeoutMs);
    }
    else
    {
        Printf(Tee::PriNormal, "Clear custom timeout. Use default value. \n");
        TimeoutMs = XHCI_DEFAULT_TIMEOUT_MS_MAGIC_WORD;
    }
    m_TimeoutMs = TimeoutMs;
    return OK;
}

/*
RC XhciComnDev::WaitMem(void *pVirtMem, UINT32 Exp, UINT32 UnExp, UINT32 TimeoutMs)
{
    LOG_ENT();
    PollMemArgs args;
    args.memAddr = reinterpret_cast<unsigned long>(pVirtMem);
    args.exp = Exp;
    args.unexp = UnExp;

    return Tasker::Poll(PollMem, (void*)(&args), TimeoutMs * m_TimeoutScaler);
}

RC XhciComnDev::WaitBitSetAnyMem(void *pVirtMem, UINT32 Exp, UINT32 TimeoutMs)
{
    LOG_ENT();
    PollMemArgs args;
    args.memAddr = reinterpret_cast<unsigned long>(pVirtMem);
    args.exp = Exp;

    return Tasker::Poll(PollMemSetAny, (void*)(&args), TimeoutMs * m_TimeoutScaler);
}
*/

