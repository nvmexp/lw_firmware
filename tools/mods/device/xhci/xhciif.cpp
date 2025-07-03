/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "cheetah/include/jstocpp.h"
#include "xhcictrlmgr.h"
#include "xhcictrl.h"
#include "xhciring.h"
#include "xhcitrb.h"
#include "usbtype.h"
#include "device/include/memtrk.h"
#include "core/include/memfrag.h"
#include "core/include/data.h"

EXPOSE_CONTROLLER_CLASS_WITH_PCI_CFG(XhciController);
EXPOSE_CONTROLLER_MGR(Xhci, XhciController, "Xhci Controller");

PROP_CONST(Xhci, RATIO_64, XHCI_RATIO_64BIT);
PROP_CONST(Xhci, RATIO_IDT, XHCI_RATIO_IDT);
PROP_CONST(Xhci, RATIO_ED, XHCI_RATIO_EVENT_DATA);

PROP_CONST(Xhci, RING_CMD, XHCI_RING_TYPE_CMD);
PROP_CONST(Xhci, RING_EVENT, XHCI_RING_TYPE_EVENT);
PROP_CONST(Xhci, RING_XFER, XHCI_RING_TYPE_XFER);

PROP_CONST(Xhci, CLASS_STOR, UsbClassMassStorage);
PROP_CONST(Xhci, CLASS_HUB, UsbClassHub);
PROP_CONST(Xhci, CLASS_HID, UsbClassHid);
PROP_CONST(Xhci, CLASS_VIDEO, UsbClassVideo);
PROP_CONST(Xhci, CLASS_CFG, UsbConfigurableDev);

PROP_CONST(Xhci, U0_IDLE, LOWPOWER_MODE_U0_Idle);
PROP_CONST(Xhci, U1, LOWPOWER_MODE_U1);
PROP_CONST(Xhci, U2, LOWPOWER_MODE_U2);
PROP_CONST(Xhci, U3, LOWPOWER_MODE_U3);
PROP_CONST(Xhci, L1, LOWPOWER_MODE_L1);
PROP_CONST(Xhci, L2, LOWPOWER_MODE_L2);

JSIF_TEST_1(XhciController, SetSSPortMap, UINT32, PortMap, "Mapping of SSPorts - HSPorts");
JSIF_TEST_1(XhciController, SetEnumMode, UINT32, BitMask, "Set enumeration mode")
//JSIF_TEST_1(XhciController, SetDebugMode, UINT32, BitMask, "Set debug mode")
JSIF_TEST_1(XhciController, SetBar2, UINT32, BAR2, "Set the BAR2 MMIO physical address")
JSIF_METHOD_1(XhciController, UINT32, Bar2Rd32, UINT32, Addr, "Read the BAR2 registers")
JSIF_TEST_2(XhciController, Bar2Wr32, UINT32, Offset, UINT32, Data32, "Write BAR2 registers")

JS_STEST_LWSTOM(XhciController, SetDebugMode, 2, "Set debug mode")
{
    JSIF_INIT_VAR(XhciController, SetDebugMode, 1, 2, DebugMode, *IsForceValue);
    JSIF_GET_ARG(0, UINT32, DebugMode);
    JSIF_GET_ARG_OPT(1, bool, IsForceValue, false);
    JSIF_CALL_TEST(XhciController, SetDebugMode, DebugMode, IsForceValue);
    JSIF_RETURN(rc);
}

JSIF_TEST_1(XhciController, SetFullCopy, bool, IsEnable, "Set full copy for Xhci.SetUserContext(). By default DeqPtr field will be handled by MODS")
JSIF_TEST_1(XhciController, SetTimeout, UINT32, TimeoutMs, "Set default timeout(ms)")
JSIF_TEST_1(XhciController, FindTrb, UINT32, TrbAddr, "Find specified TRB in Transfer Rings")

JS_STEST_LWSTOM(XhciController, Search, 2, "Search for attached USB devices")
{
    JSIF_INIT_VAR(XhciController, Search, 0, 2, *PortNo);
    JSIF_GET_ARG_OPT(0, UINT32, PortNo, 0);
    JSIF_GET_ARG_OPT(1, JSObject *, pReturlwals, NULL);

    if (NumArguments <= 1)
    {
        JSIF_CALL_TEST(XhciController, Search, PortNo);
    }
    else
    {
        vector<UINT32> vData32;
        JSIF_CALL_TEST(XhciController, Search, PortNo, &vData32);
        for ( UINT32 i = 0; i < vData32.size(); i++ )
        {
            JavaScriptPtr()->SetElement(pReturlwals, i, vData32[i]);
        }
    }
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, SetUserContext, 2, "Set lwstomized context")
{
    JSIF_INIT_VAR(XhciController, SetUserContext, 1, 2, Array32, *AlignOffset);
    JSIF_GET_ARG(0, vector<UINT32>, Array32);
    JSIF_GET_ARG_OPT(1, UINT32, AlignOffset, 0);
    JSIF_CALL_TEST(XhciController, SetUserContext, &Array32, AlignOffset);
    JSIF_RETURN(rc);
}

JSIF_TEST_2(XhciController, SetRnd, UINT32, Index, UINT32, Ratio, "Set parameters for randomization. Index: 1-RATIO_64, 2-RATIO_IDT, 3-RATIO_ED / Ratio: 0-100")

JS_STEST_LWSTOM(XhciController, UpdateTrb, 3, "Manually setup TRB's bit fields for specified Cmd/Xfer Rings for certain TRB type")
{
    JSIF_INIT_VAR(XhciController, UpdateTrb, 2, 3, TrbType, MaskDataArray32, *Times);
    JSIF_GET_ARG(0, UINT32, TrbType);
    JSIF_GET_ARG(1,  vector<UINT32>, MaskDataArray32);
    JSIF_GET_ARG_OPT(2, UINT32, Times, 1);

    JSIF_CALL_TEST_STATIC(TrbHelper, UpdateTrb, TrbType, &MaskDataArray32, Times);
    JSIF_RETURN(rc);
}

//JSIF_TEST_1(XhciController, LoadFW, string, FileName, "Load the Firmware")
JS_STEST_LWSTOM(XhciController, LoadFW, 2, "Load the Firmware")
{
    JSIF_INIT_VAR(XhciController, LoadFW, 1, 2, FileName, *IsEnMailbox);
    JSIF_GET_ARG(0, string, FileName);
    JSIF_GET_ARG_OPT(1, bool, IsEnMailbox, false);

    JSIF_CALL_TEST(XhciController, LoadFW, FileName, IsEnMailbox);
    JSIF_RETURN(rc);
}

JSIF_TEST_2(XhciController, MailboxTx, UINT32, Data, bool, IsSetId, "Mailbox write")
JSIF_TEST_0(XhciController, MailboxAckCheck, "Check Mailbox for ACK from FW")
JSIF_TEST_0(XhciController, ResetHc, "Reset host controller")
JSIF_TEST_0(XhciController, StateSave, "Srop running endpoints and save registers")
JSIF_TEST_0(XhciController, StateRestore, "Restore saved registers and re-start endpoints stopped by Xhci.StateSave()")
JSIF_TEST_1(XhciController, SetLocalReg, bool, IsEnable, "Set if Xhci.RegRd() reads from local image setup by Xhci.StateSave()")

JS_STEST_LWSTOM(XhciController, Doorbell, 4, "Manually setup TRB's bit fields for specified Cmd/Xfer Rings for certain TRB type")
{
    JSIF_INIT_VAR(XhciController, Doorbell, 1, 4, SlotId, *EpNum, *IsDataOut, *StreamId);
    JSIF_GET_ARG(0, UINT08, SlotId);
    JSIF_GET_ARG_OPT(1, UINT08, EpNum, 0);
    JSIF_GET_ARG_OPT(2, bool, IsDataOut, true);
    JSIF_GET_ARG_OPT(3, UINT32, StreamId, 0);

    JSIF_CALL_TEST(XhciController, RingDoorBell, SlotId, EpNum, IsDataOut, StreamId);
    JSIF_RETURN(rc);
}

JSIF_METHOD_1(XhciController, UINT32, FlushEvent, UINT32, EventRingIndex, "Flush the cached TRBs in specified Event Ring")
JSIF_TEST_1(XhciController, StartStop, bool, IsStart, "Toggle the Run/Stop bit of register USBCMD")
JSIF_TEST_1(XhciController, ResetPort, UINT32,  Port, "Reset Port, put attached USB device into its default state")
JSIF_TEST_0(XhciController, CmdRingReloc, "Realocate Command Ring")
JSIF_TEST_1(XhciController, XferNoOp, UINT32, Id, "Issue NoOp Xfer TRB to specific Xfer Ring")
JSIF_TEST_3(XhciController, ResetEndpoint, UINT32, SlotId, UINT32, EpNum, bool, IsDataOut, "Reset endpoints in Halted state")

JS_STEST_LWSTOM(XhciController, WaitXfer, 4, "Wait for Transfer Conpletion for specified Endpoint")
{
    JSIF_INIT_VAR(XhciController, WaitXfer, 3, 4, SlotId, EpNum, IsDataOut, *IsWaitAll);
    JSIF_GET_ARG(0, UINT08, SlotId);
    JSIF_GET_ARG(1, UINT08, EpNum);
    JSIF_GET_ARG(2, bool, IsDataOut);
    JSIF_GET_ARG_OPT(3, bool, IsWaitAll, true);

    UINT32 timeoutMs =((XhciController*) JS_GetPrivate(pContext, pObject))->GetTimeout();
    JSIF_CALL_TEST(XhciController, WaitXfer, SlotId, EpNum, IsDataOut, timeoutMs, NULL, IsWaitAll);
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, WaitEventType, 2, "Wait for specified Event type in Event Ring")
{
    JSIF_INIT_VAR(XhciController, WaitEventType, 2, 3, EventRingId, EventType, *[&Trb]);
    JSIF_GET_ARG(0, UINT32, RingId);
    JSIF_GET_ARG(1, UINT08, EventType);
    UINT32 timeoutMs =((XhciController*) JS_GetPrivate(pContext, pObject))->GetTimeout();

    XhciTrb myTrb;
    JSIF_CALL_TEST(XhciController, WaitForEvent, RingId, EventType, timeoutMs, &myTrb);
    if (NumArguments > 2)
    {
        JSIF_GET_ARG(2, JSObject *, pReturlwals);
        if ( OK != (rc = JavaScriptPtr()->SetElement(pReturlwals, 0, myTrb.DW0)) )
        { JS_ReportError(pContext, "Couldn't colwert result.");return JS_FALSE; }
        if ( OK != (rc = JavaScriptPtr()->SetElement(pReturlwals, 1, myTrb.DW1)) )
        { JS_ReportError(pContext, "Couldn't colwert result.");return JS_FALSE; }
        if ( OK != (rc = JavaScriptPtr()->SetElement(pReturlwals, 2, myTrb.DW2)) )
        { JS_ReportError(pContext, "Couldn't colwert result.");return JS_FALSE; }
        if ( OK != (rc = JavaScriptPtr()->SetElement(pReturlwals, 3, myTrb.DW3)) )
        { JS_ReportError(pContext, "Couldn't colwert result.");return JS_FALSE; }
    }
    else
    {
        if (OK == rc)
        {
            TrbHelper::DumpTrbInfo(&myTrb, true, Tee::PriNormal);
        }
    }

    JSIF_RETURN(rc);
}

JSIF_TEST_1(XhciController, SetEventData, bool, IsEnable, "Toggle flag of Event Data TRB insertion")

JS_STEST_LWSTOM(XhciController, CmdStop, 1, "Command Stop")
{
    JSIF_INIT_VAR(XhciController, CmdStop, 0, 1, *IsWaitForEvent);
    JSIF_GET_ARG_OPT(0, bool, IsWaitForEvent, true);
    JSIF_CALL_TEST(XhciController, CommandStopAbort, true, IsWaitForEvent);
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, CmdAbort, 1, "Command Abort")
{
    JSIF_INIT_VAR(XhciController, CmdAbort, 0, 1, *IsWaitForEvent);
    JSIF_GET_ARG_OPT(0, bool, IsWaitForEvent, true);
    JSIF_CALL_TEST(XhciController, CommandStopAbort, false, IsWaitForEvent);
    JSIF_RETURN(rc);
}

JSIF_TEST_0(XhciController, CmdNoOp, "Issue NoOp command to verify Command/Event Ring function correctly")
JSIF_METHOD_0(XhciController, UINT08, CmdEnableSlot, "Send Enable Slot command and return the Slot ID")
JSIF_TEST_1(XhciController, CmdDisableSlot, UINT08, SlotID, "Send Disable Slot command for specified Slot ID")
JSIF_TEST_1(XhciController, SetupDeviceContext, UINT08, SlotID, "Setup blank Device Context for specified Slot ID, needed by CmdAddressDev")

JS_SMETHOD_LWSTOM(XhciController, CmdAddressDev, 2, "Setup and send Address Device command for specified Slot ID")
{
    JSIF_INIT_VAR(XhciController, CmdAddressDev, 1, 2, SlotId, *IsBsr);
    JSIF_GET_ARG(0, UINT08, SlotId);
    JSIF_GET_ARG_OPT(1, bool, IsBsr, false);

    UINT08 usbAddr;
    if (OK != ((XhciController*) JS_GetPrivate(pContext, pObject))->CmdAddressDev(0, 0, 1, SlotId, &usbAddr, IsBsr))
    {
        JS_ReportError(pContext, "Error in Xhci.CmdAddressDev()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    JSIF_RETURN(usbAddr);
}

JS_STEST_LWSTOM(XhciController, CmdCfgEndpoint, 2, "Setup and send Configure Endpoint command for specified Slot ID")
{
    JSIF_INIT_VAR(XhciController, CmdCfgEndpoint, 1, 2, SlotId, *IsDc);
    JSIF_GET_ARG(0, UINT08, SlotId);
    JSIF_GET_ARG_OPT(1, bool, IsDc, false);
    JSIF_CALL_TEST(XhciController, CmdCfgEndpoint, SlotId, NULL, IsDc);
    JSIF_RETURN(rc);
}

JSIF_TEST_1(XhciController, CmdEvalContext, UINT32, SlotID, "Send Evaluate Context command to specified Slot ID")

JSIF_TEST_3(XhciController, CmdResetEndpoint, UINT08, SlotId, UINT08, EpDci, bool, IsTsp, "Setup and send Configure Endpoint command for specified Slot ID")
JSIF_TEST_3(XhciController, CmdStopEndpoint, UINT08, SlotId, UINT08, EpDci, bool, IsSp, "Issue Stop Endpoint command")

JS_STEST_LWSTOM(XhciController, CmdSetTrDeqPtr, 6, "Issue Set TR Dequeue Pointer Command command")
{
    JSIF_INIT_VAR(XhciController, CmdSetTrDeqPtr, 4, 6, SlotId, EpDci, PhyAddr, IsDcs, *StreamId, *Sct);
    JSIF_GET_ARG(0, UINT08, SlotId);
    JSIF_GET_ARG(1, UINT08, EpDci);
    JSIF_GET_ARG(2, UINT64, PhyAddr);
    JSIF_GET_ARG(3, bool, IsDcs);
    JSIF_GET_ARG_OPT(4, UINT32, StreamId, 0);
    JSIF_GET_ARG_OPT(5, UINT08, Sct, 0);
    JSIF_CALL_TEST(XhciController, CmdSetTRDeqPtr, SlotId, EpDci, PhyAddr, IsDcs, StreamId, Sct);
    JSIF_RETURN(rc);
}

JSIF_TEST_1(XhciController, CmdResetDevice, UINT32, SlotID, "Issue Reset Device command for specified SlotId")

JS_STEST_LWSTOM(XhciController, CmdGetPortBw, 3, "Issue Set TR Dequeue Pointer Command command")
{
    JSIF_INIT_VAR(XhciController, CmdGetPortBw, 2, 3, DevSpeed, HubSlotId, *[Data08 &]);
    JSIF_GET_ARG(0, UINT08, DevSpeed);
    JSIF_GET_ARG(1, UINT08, HubSlotId);
    if (NumArguments < 3)
    {
        JSIF_CALL_TEST(XhciController, CmdGetPortBw, DevSpeed, HubSlotId);
    }
    else
    {
        JSIF_GET_ARG(2, JSObject *, pReturlwals);
        vector<UINT08> vData08;
        JSIF_CALL_TEST(XhciController, CmdGetPortBw, DevSpeed, HubSlotId, &vData08);
        for ( UINT32 i = 0; i < vData08.size(); i++ )
        {
            JavaScriptPtr()->SetElement(pReturlwals, i, vData08[i]);
        }
    }
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, CmdForceHeader, 5, "Issue Force Header command")
{
    JSIF_INIT(XhciController, CmdForceHeader, 5, Lo, Mid, Hi, Type, PortNum);
    JSIF_GET_ARG(0, UINT32, Lo);
    JSIF_GET_ARG(1, UINT32, Mid);
    JSIF_GET_ARG(2, UINT32, Hi);
    JSIF_GET_ARG(3, UINT08, Type);
    JSIF_GET_ARG(4, UINT08, PortNum);
    JSIF_CALL_TEST(XhciController, CmdForceHeader, Lo, Mid, Hi, Type, PortNum);
    JSIF_RETURN(rc);
}

JSIF_METHOD_1(XhciController, UINT32, CsbRd, UINT32, Addr, "Read the CSB registers")
JSIF_TEST_2(XhciController, CsbWr, UINT32, Addr, UINT32, Data32, "Write CSB registers")

JS_STEST_LWSTOM(XhciController, UsbCfg, 3, "Congifuration the USB device attached on given port")
{
    JSIF_INIT_VAR(XhciController, UsbCfg, 1, 4, Port, *RouteString, *IsLoadClassDriver, *UsbSpeed);
    JSIF_GET_ARG(0, UINT08, Port);
    JSIF_GET_ARG_OPT(1, UINT32, RouteString, 0);
    JSIF_GET_ARG_OPT(2, bool, IsLoadClassDriver, true);
    JSIF_GET_ARG_OPT(3, UINT32, UsbSpeed, 0);
    JSIF_CALL_TEST(XhciController, UsbCfg, Port, IsLoadClassDriver, RouteString, UsbSpeed);
    JSIF_RETURN(rc);
}

JSIF_TEST_1(XhciController, DeCfg, UINT32, SlotId, "De-Congifuration specified slot, send Disable Slot Command & release resource")
JSIF_METHOD_0(XhciController, UINT32, GetNewSlot, "Get the Slot ID newly addsigned by HC")

JS_STEST_LWSTOM(XhciController, GetDevSlots, 1, "Get the array of Slot IDs already allocated by HC")
{
    JSIF_INIT_VAR(XhciController, GetDevSlots, 0, 1, *[Data08 &]);
    if (NumArguments < 1)
    {
        JSIF_CALL_TEST(XhciController, GetDevSlots);
    }
    else
    {
        JSObject * pReturlwals;
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &pReturlwals) )
        {  JS_ReportError(pContext, "Failed to get parameter."); return JS_FALSE; }

        vector<UINT08> vData08;
        JSIF_CALL_TEST(XhciController, GetDevSlots, &vData08);
        for ( UINT32 i = 0; i < vData08.size(); i++ )
        {
            JavaScriptPtr()->SetElement(pReturlwals, i, vData08[i]);
        }
    }
    JSIF_RETURN(rc);
}

JSIF_METHOD_1(XhciController, UINT32, GetDevClass, UINT08, SlotId, "Get class of the USB device")
JSIF_TEST_1(XhciController, SetCmdRingSize, UINT32, Size, "Set default size of Command Ring")
JSIF_TEST_1(XhciController, SetEventRingSize, UINT32, Size, "Set default size of Event Ring")
JSIF_TEST_1(XhciController, SetXferRingSize, UINT32, Size, "Set default size of Xfer Ring")
JSIF_TEST_1(XhciController, SetCErr, UINT08, Cerr, "Set CErr override of EP Context");
JSIF_TEST_1(XhciController, EnableBto, UINT08, IsEnable, "Switch of BTO feature");

JSIF_METHOD_0(XhciController, UINT32, FindCmdRing, "Get the ID for Command Ring")
JSIF_METHOD_1(XhciController, UINT32, FindEventRing, UINT32, EventIndex, "Get the ID for Event Ring x")

JS_STEST_LWSTOM(XhciController, TrTd, 4, "Truncate TD into segments")
{
    JSIF_INIT(XhciController, TrTd, 4, SlotId, EpNum, IsDataOut, Num);
    JSIF_GET_ARG(0, UINT32, SlotId);
    JSIF_GET_ARG(1, UINT32, EpNum);
    JSIF_GET_ARG(2, bool, IsDataOut);
    JSIF_GET_ARG(3, UINT32, Num);

    JSIF_CALL_TEST(XhciController, TrTd, SlotId, EpNum, IsDataOut, Num);
    JSIF_RETURN(rc);
}

JS_SMETHOD_LWSTOM(XhciController, FindXferRing, 4, "Get the ID for Xfer Ring for given Slot ID / Endpoint / DataInOut")
{
    JSIF_INIT_VAR(XhciController, FindXferRing, 3, 4, SlotId, EpNum, IsDataOut, *StreamId);
    JSIF_GET_ARG(0, UINT08, SlotId);
    JSIF_GET_ARG(1, UINT08, EpNum);
    JSIF_GET_ARG(2, bool, IsDataOut);
    JSIF_GET_ARG_OPT(3, UINT32, StreamId, 0);

    UINT32 ringId;
    if ( OK != ((XhciController*) JS_GetPrivate(pContext, pObject))->FindXferRing(SlotId, EpNum, IsDataOut, &ringId, StreamId) )
    {
        JS_ReportError(pContext, "Error in Xhci.FindXferRing()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    JSIF_RETURN(ringId);
}

JSIF_TEST_2(XhciController, RingAppend, UINT32, RingUuid, UINT32, SegSize, "Append one new segment to specific Ring")
JSIF_TEST_2(XhciController, RingRemove, UINT32, RingUuid, UINT32, SegIndex, "Remove specific segment from Ring")

JS_STEST_LWSTOM(XhciController, GetRingPtr, 3, "Get current Enq/Deq Pointor of specific Ring")
{
    JSIF_INIT_VAR(XhciController, GetRingPtr, 2, 3, RingId, [Arr &], *BackLevel);
    JSIF_GET_ARG(0, UINT32, RingId);
    JSIF_GET_ARG(1, JSObject *, pReturlwals);
    JSIF_GET_ARG_OPT(2, UINT32, BackLevel, 0);

    UINT32 segment, index;
    PHYSADDR physAddr = 0;
    C_CHECK_RC(((XhciController*) JS_GetPrivate(pContext, pObject))->GetRingPtr(RingId, &segment, &index, &physAddr, BackLevel));
    JavaScriptPtr()->SetElement(pReturlwals, 0, segment);
    JavaScriptPtr()->SetElement(pReturlwals, 1, index);
    UINT32 tmp = physAddr;
    JavaScriptPtr()->SetElement(pReturlwals, 2, tmp);
    physAddr = physAddr >> 32;
    tmp = physAddr;
    JavaScriptPtr()->SetElement(pReturlwals, 3, tmp);
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, SetRingPtr, 4,  "Set Enq Pointer of specific Xfer/Cmd Ring")
{
    JSIF_INIT_VAR(XhciController, SetRingPtr, 3, 4, RingId, Segment, Index, *IsCyclBit);
    JSIF_GET_ARG(0, UINT32, RingId);
    JSIF_GET_ARG(1, UINT32, Segment);
    JSIF_GET_ARG(2, UINT32, Index);
    JSIF_GET_ARG_OPT(3, bool, IsCyclBit, false);
    JSIF_CALL_TEST(XhciController, SetRingPtr, RingId, Segment, Index, IsCyclBit?1:-1);
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, GetTrb, 4, "Get specific TRB content")
{
    JSIF_INIT_VAR(XhciController, GetTrb, 3, 4, RingId, SegIndex, TrbIndex, *[Data32 &]);
    JSIF_GET_ARG(0, UINT32, RingId);
    JSIF_GET_ARG(1, UINT32, SegIndex);
    JSIF_GET_ARG(2, UINT32, TrbIndex);

    XhciTrb myTrb;
    JSIF_CALL_TEST(XhciController, GetTrb, RingId, SegIndex, TrbIndex, &myTrb);
    if (NumArguments > 3)
    {
        JSIF_GET_ARG(3, JSObject *, pReturlwals);
        if ( OK != (rc = JavaScriptPtr()->SetElement(pReturlwals, 0, myTrb.DW0)) )
        { JS_ReportError(pContext, "Couldn't colwert result.");return JS_FALSE; }
        if ( OK != (rc = JavaScriptPtr()->SetElement(pReturlwals, 1, myTrb.DW1)) )
        { JS_ReportError(pContext, "Couldn't colwert result.");return JS_FALSE; }
        if ( OK != (rc = JavaScriptPtr()->SetElement(pReturlwals, 2, myTrb.DW2)) )
        { JS_ReportError(pContext, "Couldn't colwert result.");return JS_FALSE; }
        if ( OK != (rc = JavaScriptPtr()->SetElement(pReturlwals, 3, myTrb.DW3)) )
        { JS_ReportError(pContext, "Couldn't colwert result.");return JS_FALSE; }
    }
    else
    {
        TrbHelper::DumpTrbInfo(&myTrb, true, Tee::PriNormal);
    }
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, SetTrb, 4, "Set specific TRB content")
{
    JSIF_INIT_VAR(XhciController, SetTrb, 3, 4, RingId, SegIndex, TrbIndex, [Data32]);
    JSIF_GET_ARG(0, UINT32, RingId);
    JSIF_GET_ARG(1, UINT32, SegIndex);
    JSIF_GET_ARG(2, UINT32, TrbIndex);
    JSIF_GET_ARG(3, vector<UINT32>, TrbData);

    if (TrbData.size() != 4)
    {
        JS_ReportError(pContext, "[Data32] should constain 4 elements" );
        return JS_FALSE;
    }
    XhciTrb myTrb;
    myTrb.DW0 = TrbData[0];
    myTrb.DW1 = TrbData[1];
    myTrb.DW2 = TrbData[2];
    myTrb.DW3 = TrbData[3];

    JSIF_CALL_TEST(XhciController, SetTrb, RingId, SegIndex, TrbIndex, &myTrb);
    JSIF_RETURN(rc);
}

JSIF_METHOD_1(XhciController, UINT32, GetRingType, UINT32, RingId, "Get the type for Ring x")

JS_STEST_LWSTOM(XhciController, GetRingSize, 2, "Get array of segments' size of specific Ring")
{
    JSIF_INIT_VAR(XhciController, GetRingSize, 1, 2, RingId, *[Data &]);
    JSIF_GET_ARG(0, UINT32, RingId);

    if (NumArguments > 0)
    {
        JSIF_GET_ARG(1, JSObject *, pReturlwals);
        vector<UINT32> vData32;
        JSIF_CALL_TEST(XhciController, GetRingSize, RingId, &vData32);
        for ( UINT32 i = 0; i < vData32.size(); i++ )
        {
            JavaScriptPtr()->SetElement(pReturlwals, i, vData32[i]);
        }
    }
    else
    {
        JSIF_CALL_TEST(XhciController, GetRingSize, RingId, NULL);
    }
    JSIF_RETURN(rc);
}

JSIF_METHOD_1(XhciController, UINT32, GetIntrTarget, UINT32, RingId, "Get the Interrupt Target setting for given Xfer Ring")
JSIF_TEST_2(XhciController, SetIntrTarget, UINT32, RingId, UINT32, IntrTarget, "Set the default Interrupt Target for given Xfer Ring")

JS_STEST_LWSTOM(XhciController, InitXferRing, 5, "Initialize Xfer Ring for specific Pipe")
{
    JSIF_INIT_VAR(XhciController, InitXferRing, 3, 5, SlotId, EpNum, IsDataOut, *StreamId, *[AddrLo AddrHi &]);
    JSIF_GET_ARG(0, UINT08, SlotId);
    JSIF_GET_ARG(1, UINT08, EpNum);
    JSIF_GET_ARG(2, bool, IsDataOut);
    JSIF_GET_ARG_OPT(3, UINT32, StreamId, 0);

    if ( NumArguments > 4)
    {   // user wants to manually create a xfer ring.
        // this happens when user wants to get the address to fill into input context
        JSObject * pReturlwals;
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[4], &pReturlwals) )
        { JS_ReportError(pContext, "Failed to get parameter."); return JS_FALSE; }
        // todo: add RemoveRingObj if needed
        TransferRing * pXferRing = new TransferRing(SlotId, EpNum, IsDataOut, StreamId);
        C_CHECK_RC(pXferRing->Init(200));
        JSIF_CALL_TEST(XhciController, SaveXferRing, pXferRing);

        PHYSADDR pPhyBase;
        CHECK_RC(pXferRing->GetBaseAddr(&pPhyBase));

        UINT32 tmp = pPhyBase;
        JavaScriptPtr()->SetElement(pReturlwals, 0, tmp);
        pPhyBase >>= 32;
        tmp = pPhyBase;
        JavaScriptPtr()->SetElement(pReturlwals, 1, tmp);
    }
    else
    {
        JSIF_CALL_TEST(XhciController, InitXferRing, SlotId, EpNum, IsDataOut, StreamId);
    }
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, GetDevContext, 5, "Get specific Device Content")
{
    JSIF_INIT_VAR(XhciController, GetDevContext, 1, 2, SlotId, *[Data32 &]);
    JSIF_GET_ARG(0, UINT08, SlotId);

    if ( NumArguments > 0)
    {
        JSObject * pReturlwals;
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[1], &pReturlwals) )
        { JS_ReportError(pContext, "Failed to get parameter."); return JS_FALSE; }
        vector<UINT32> vData32;
        JSIF_CALL_TEST(XhciController, GetDevContext, SlotId, &vData32);
        for ( UINT32 i = 0; i < vData32.size(); i++ )
        {
            JavaScriptPtr()->SetElement(pReturlwals, i, vData32[i]);
        }
    }
    else
    {
        JSIF_CALL_TEST(XhciController, GetDevContext, SlotId);
    }
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, SetDevContext, 2, "Set specific Device Content")
{

    JSIF_INIT(XhciController, SetDevContext, 2, SlotId, [Data32]);
    JSIF_GET_ARG(0, UINT32, SlotId);
    JSIF_GET_ARG(1, vector<UINT32>, Data32);

    JSIF_CALL_TEST(XhciController, SetDevContext, SlotId, &Data32);
    JSIF_RETURN(rc);
}

// Stream
//MCP_JSCLASS_TEST(Xhci, SetupStream, 2, "Setup Stream layout on runtime")
//MCP_JSCLASS_TEST(Xhci, InitStream, 3, "Initialize Stream")

// Info
//MCP_JSCLASS_TEST(Xhci, PrintData, 2, "Print HC data structures")
//MCP_JSCLASS_TEST(Xhci, PrintRing, 1, "Print content of specific Ring")
JSIF_TEST_1(XhciController, PrintRegPort, UINT32, Port, "Print port registers")
JSIF_TEST_0(XhciController, DumpAll, "Dump all the registers & Device Contexts & Event Ring")
//MCP_JSCLASS_TEST(Xhci, DumpRing, 2, "Dump the Command / Transfer Ring containing the specified TRB")

// Class driver related
//MCP_JSCLASS_METHOD(Xhci, GetCmdCbw, 1, "Return the CBW for SCSI commands MODS generated")
//MCP_JSCLASS_TEST(Xhci, ReadSector, 4, "Read Sectors")
//MCP_JSCLASS_TEST(Xhci, WriteSector, 4, "Write Sectors")
JSIF_TEST_1(XhciController, CmdTestUnitReady, UINT08, SlotId, "Issue Test Unit Ready command to USB storage device")
JSIF_TEST_1(XhciController, CmdModeSense, UINT08, SlotId, "Issue ModeSense SCSI command to USB storage device")
JSIF_TEST_1(XhciController, CmdInquiry, UINT08, SlotId, "Issue Inquiry SCSI command to USB storage device")
JSIF_METHOD_1(XhciController, UINT32, GetMaxLba, UINT08, SlotId, "Issue GetMaxLba command to USB storage device")
JS_STEST_LWSTOM(XhciController, CmdReadCapacity, 2, "Issue ReadCapacity SCSI command")
{
    JSIF_INIT(XhciController, CmdReadCapacity, 2, SlotId, [Data &]);
    JSIF_GET_ARG(0, UINT08, SlotId);
    UINT32 maxLBA;
    UINT32 sectorSize;
    JSIF_CALL_TEST(XhciController, CmdReadCapacity, SlotId, &maxLBA, &sectorSize);
    JSIF_GET_ARG(1, JSObject *, pReturlwals);
    if ( OK != (rc = JavaScriptPtr()->SetElement(pReturlwals, 0, maxLBA)) )
    {
        JS_ReportError(pContext, "Couldn't colwert result.");
        return JS_FALSE;
    }
    if ( OK != (rc = JavaScriptPtr()->SetElement(pReturlwals, 1, sectorSize)) )
    {
        JS_ReportError(pContext, "Couldn't colwert result.");
        return JS_FALSE;
    }
    JSIF_RETURN(rc);
}
JS_STEST_LWSTOM(XhciController, CmdReadFormatCapacity, 2, "Issue ReadFormatCapacity SCSI command")
{
    JSIF_INIT(XhciController, CmdReadFormatCapacity, 2, SlotId, [Data &]);
    JSIF_GET_ARG(0, UINT08, SlotId);
    UINT32 numBlocks;
    UINT32 blockSize;
    JSIF_CALL_TEST(XhciController, CmdReadFormatCapacity, SlotId, &numBlocks, &blockSize);
    JSIF_GET_ARG(1, JSObject *, pReturlwals);
    if ( OK != (rc = JavaScriptPtr()->SetElement(pReturlwals, 0, numBlocks)) )
    {
        JS_ReportError(pContext, "Couldn't colwert result.");
        return JS_FALSE;
    }
    if ( OK != (rc = JavaScriptPtr()->SetElement(pReturlwals, 1, blockSize)) )
    {
        JS_ReportError(pContext, "Couldn't colwert result.");
        return JS_FALSE;
    }
    JSIF_RETURN(rc);
}
//MCP_JSCLASS_TEST(Xhci,  ReadMouse, 2, "Read mouse")
//MCP_JSCLASS_TEST(Xhci,  ReadWebCam, 4, "Read webcam")
//MCP_JSCLASS_TEST(Xhci, SaveFrame, 1, "Save Frame")
//MCP_JSCLASS_METHOD(Xhci,  DevRegRd, 2, "Read register of configurable USB device")
//MCP_JSCLASS_TEST(Xhci,  DevRegWr, 3, "Write register of configurable USB device")
//MCP_JSCLASS_TEST(Xhci,  DataIn, 3, "Read data from configurable USB device")
//MCP_JSCLASS_TEST(Xhci,  DataOut, 3, "Write data to configurable USB device")
//MCP_JSCLASS_TEST(Xhci,  DataDone, 3, "Retrieve the data associated with specified Transfer Ring and relase the memory")
//MCP_JSCLASS_TEST(Xhci,  InsertTd, 3, "Insert TRBs into specified Transfer Ring")
JSIF_TEST_1(XhciController, InitDummyBuf, UINT32, BufferSize, "Initialize dummy memory buffer for used by InsertTd()")
//MCP_JSCLASS_TEST(Xhci, SendIsoCmd, 2, "Issue vendor defined Isoch Commands (0-start,1-stop,2-query,3-reset) ")

// USB requesters
//MCP_JSCLASS_TEST(Xhci, ReqGetDescriptor, 4, "Issue USB GetDescriptor request")
//MCP_JSCLASS_TEST(Xhci, ReqClearFeature, 4, "Issue USB ClearFeature request")
JSIF_METHOD_1(XhciController, UINT08, ReqGetConfiguration, UINT08, SlotId, "Issue USB GetConfiguration request and return the Index")
//MCP_JSCLASS_METHOD(Xhci, ReqGetInterface, 2, "Issue USB GetInterface request")
//MCP_JSCLASS_METHOD(Xhci, ReqGetStatus, 3, "Issue USB GetStatus request")
//MCP_JSCLASS_TEST(Xhci, ReqSetConfiguration, 2, "Issue USB SetConfiguration request")
//MCP_JSCLASS_TEST(Xhci, ReqSetFeature, 6, "Issue USB SetFeature request")
//MCP_JSCLASS_TEST(Xhci, ReqSetInterface, 3, "Issue USB SetInterface request")

// Hub requesters
//MCP_JSCLASS_TEST(Xhci, HubReqClearHubFeature, 2, "Issue USB HubClearHubFeature request")
//MCP_JSCLASS_TEST(Xhci, HubReqClearPortFeature, 4, "Issue USB HubClearPortFeature request")
//MCP_JSCLASS_TEST(Xhci, HubReqGetHubDescriptor, 2, "Issue USB GetHubStatus request")
//MCP_JSCLASS_METHOD(Xhci, HubReqGetHubStatus, 1, "Issue USB GetHubStatus request")
//MCP_JSCLASS_METHOD(Xhci, HubReqGetPortStatus, 2, "Issue USB HubGetPortStatus request")
//MCP_JSCLASS_METHOD(Xhci, HubReqPortErrorCount, 2, "Issue USB HubPortErrorCount request")
//MCP_JSCLASS_TEST(Xhci, HubReqSetHubFeature, 2, "Issue USB HubSetHubFeature request")
//MCP_JSCLASS_TEST(Xhci, HubReqSetHubDepth, 2, "Issue USB HubSetHubDepth request")
//MCP_JSCLASS_TEST(Xhci, HubReqSetPortFeature, 4, "Issue USB HubSetPortFeature request")

//...
//MCP_JSCLASS_METHOD(Xhci, Wr64Bit, 1, "64bit integer bit field operation")
//MCP_JSCLASS_METHOD(Xhci, CheckXferEvent, 5, "Check if there's Xfer Event for specific Endpoint arrived");

JS_STEST_LWSTOM(XhciController, ReadHub, 2, "Set lwstomized context")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if (NumArguments < 1 || NumArguments > 2)
    {
        JS_ReportError(pContext,"Usage: Xhci.ReadHub(SlotId, *MaxSeconds(0: infinite)");
        return JS_FALSE;
    }
    UINT32 slodId;
    UINT32 maxSecs = 0;
    //JsArray  jsData;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &slodId))
    { JS_ReportError(pContext, "Failed to get SlotId.");  return JS_FALSE; }

    if (NumArguments == 2)
    {
        if (OK != JavaScriptPtr()->FromJsval(pArguments[1], &maxSecs))
        {
            JS_ReportError(pContext, "Js Arg bad value");
            return JS_FALSE;
        }
    }

    JSIF_CALL_TEST(XhciController, ReadHub, slodId, maxSecs);
    JSIF_RETURN(rc);
}

JSIF_TEST_1(XhciController, HubSearch, UINT32, SlodId, "print port status")
JSIF_TEST_2(XhciController, HubReset, UINT32, SlodId, UINT08, Port,"reset HUB port")
JSIF_TEST_2(XhciController, SetConfiguration, UINT32, SlotId, UINT32, CfgIndex, "Switch Config used by HC and issue SetConfiguration request")

JS_STEST_LWSTOM(XhciController, ReadMouse, 2, "Read mouse")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if (NumArguments < 1 || NumArguments > 2)
    {
        JS_ReportError(pContext,"Usage: Xhci.ReadMouse(SlotId, *RunningTimeSec(0: infinite))");
        return JS_FALSE;
    }
    UINT32 slotId;
    UINT32 maxSecs = 0;

    //JsArray  jsData;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId))
    { JS_ReportError(pContext, "Failed to get SlotId.");  return JS_FALSE; }

    if (NumArguments == 2)
    {
        if (OK != JavaScriptPtr()->FromJsval(pArguments[1], &maxSecs))
        {
            JS_ReportError(pContext, "Js Arg bad value");
            return JS_FALSE;
        }
    }

    JSIF_CALL_TEST(XhciController, ReadMouse, slotId, maxSecs);
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, ReadWebCam, 2, "Read frames from webcam")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if (NumArguments < 1 || NumArguments > 4)
    {
        JS_ReportError(pContext,"Usage: Xhci.ReadWebCam(SlotId, *MaxSeconds(0: infinite),"
                    "MaxRunTdsPermited*(0: no limit), *MaxAddTdsPermited(0: don't care))");
        return JS_FALSE;
    }
    UINT32 slotId;
    UINT32 maxSecs = 0;
    UINT32 maxRunTds = 0;
    UINT32 maxAddTds = 0;

    //JsArray  jsData;
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId))
    { JS_ReportError(pContext, "Failed to get SlotId.");  return JS_FALSE; }

    if (NumArguments >= 2)
    {
        if (OK != JavaScriptPtr()->FromJsval(pArguments[1], &maxSecs))
        {
            JS_ReportError(pContext, "Js Arg bad value");
            return JS_FALSE;
        }
    }

    if (NumArguments >= 3)
    {
            if (OK != JavaScriptPtr()->FromJsval(pArguments[2], &maxRunTds))
            {
                JS_ReportError(pContext, "Js Arg bad value");
                return JS_FALSE;
            }
    }

    if (NumArguments >= 4)
    {
        if (OK != JavaScriptPtr()->FromJsval(pArguments[3], &maxAddTds))
        {
            JS_ReportError(pContext, "Failed to get parameters.");
            return JS_FALSE;
        }
    }

    JSIF_CALL_TEST(XhciController, ReadWebCam, slotId, maxSecs, maxRunTds, maxAddTds);
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, SaveFrame, 2, "Save frame to file")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if (NumArguments != 2)
    {
        JS_ReportError(pContext,"Usage: Xhci.SaveFrame(SlotId, FileName)");
        return JS_FALSE;
    }
    UINT32 slotId;
    string fileName;

    if ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId))
    { JS_ReportError(pContext, "Failed to get SlotId.");  return JS_FALSE; }

    if ( OK != JavaScriptPtr()->FromJsval(pArguments[1], &fileName))
    { JS_ReportError(pContext, "Failed to get FileName.");  return JS_FALSE; }

    JSIF_CALL_TEST(XhciController, SaveFrame, slotId, fileName);
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, ReadSector, 5, "Read sectors from USB storage devices")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( (NumArguments < 1) || (NumArguments > 5) )
    {
        JS_ReportError(pContext,"Usage: Xhci.ReadSector(SlotId, Lba, NumSectors, *[DataArray &], *[FixedPRD])"); return JS_FALSE;
    }
    UINT32 slotId;
    UINT32 lba = 0x100;
    UINT32 numSectors = 1;
    JSObject * pReturlwals;
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId) )
    { JS_ReportError(pContext, "Failed to get SlotId.");  return JS_FALSE; }
    if ( NumArguments >= 2 )
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[1], &lba) )
        {
            JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
        }
    if ( NumArguments >= 3 )
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[2], &numSectors) )
        {
            JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
        }
    if ( NumArguments >= 4 )
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[3], &pReturlwals) )
        {
            JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
        }

    UINT32 sectorSize;
    JSIF_CALL_TEST(XhciController, GetSectorSize, slotId, &sectorSize);
    void * pVa;
    UINT32 addrBit = 32;
    C_CHECK_RC(MemoryTracker::AllocBuffer(numSectors * sectorSize, &pVa, true, addrBit, Memory::UC));
    Memory::Fill32(pVa, 0xffffffff, numSectors * sectorSize / 4);

    //setup fixed PRD if present
    vector<UINT32> vFixedPrd;
    JsArray jsFixedPrd;
    UINT32 dataToTruncate;
    UINT32 totalByte = 0;
    if ( NumArguments >= 5 )
    {
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[4], &jsFixedPrd) )
        {
            JS_ReportError(pContext, "FixedPRD Arg bad value"); return JS_FALSE;
        }

        for ( UINT32 i = 0; i< jsFixedPrd.size(); i++ )
        {
            if ( OK != JavaScriptPtr()->FromJsval((jsFixedPrd[i]), &dataToTruncate) )
            {
                JS_ReportError(pContext, "data element bad value."); return JS_FALSE;
            }
            totalByte += dataToTruncate;
            vFixedPrd.push_back(dataToTruncate);
        }
        if ( totalByte != numSectors * sectorSize )
        {
            Printf(Tee::PriHigh,
                   "ERROR: Fixed PRD byte length %d doesn't match data size %d",
                   totalByte, numSectors * sectorSize);
            return JS_FALSE;
        }
    }
    // Get total transfer byte size
    totalByte = numSectors * sectorSize;

    // construct fraglist
    MemoryFragment::FRAGLIST fragList;
    if ( 0 == vFixedPrd.size() )
    {   //no fixed PRD specified
        void* pDataBuf = pVa;
        MemoryFragment::FRAGMENT memFrag;
        UINT32 fragSize;
        JSIF_CALL_TEST(XhciController, GetOptimisedFragLength);
        fragSize = rc;  // hack for JSIF macros
        while ( totalByte )
        {
            UINT32 txSize = (totalByte > fragSize)?fragSize:totalByte;
            memFrag.Address = pDataBuf;
            memFrag.ByteSize = txSize;
            fragList.push_back(memFrag);

            pDataBuf = (void*) ((UINT08*)pDataBuf + txSize);
            totalByte -= txSize;
        }
    }
    else
    {
        MemoryFragment::GenerateFixedFragment(pVa,
                                              0, totalByte,
                                              &fragList, 1, vFixedPrd);
    }

    UINT32 debugMode = ((XhciController*) JS_GetPrivate(pContext, pObject))->GetDebugMode();
    if (XHCI_DEBUG_MODE_USE_SCSI_CMD16 & debugMode)
    {
        JSIF_CALL_TEST(XhciController, CmdRead16, slotId, lba, numSectors, &fragList);
    }
    else
    {
        JSIF_CALL_TEST(XhciController, CmdRead10, slotId, lba, numSectors, &fragList);
    }
    if ( rc == OK )
    {
        if ( NumArguments >= 4 )
        {
            totalByte = 0;
            for ( UINT32 i = 0; i < fragList.size(); i++ )
            {
                UINT08 * pTmp = (UINT08*) fragList[i].Address;
                for ( UINT32 j = 0; j < fragList[i].ByteSize; j++ )
                {
                    JavaScriptPtr()->SetElement(pReturlwals, totalByte, MEM_RD08(pTmp+j));
                    totalByte++;
                }
            }
        }
        else
            MemoryFragment::Print32(&fragList);
    }
    MemoryTracker::FreeBuffer(pVa);
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, WriteSector, 5, "Write Sectors to USB storage devices")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( NumArguments < 1 || NumArguments > 5 )
    {
        JS_ReportError(pContext,"Usage: Xhci.WriteSector(SlotId, *Lba, *NumSectors, *[Pattern32], *[FixedPRD])");
        return JS_FALSE;
    }
    UINT32 slotId;
    UINT32 lba = 0x100;
    UINT32 numSectors = 1;
    JsArray  jsData;
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId) )
    { JS_ReportError(pContext, "Failed to get SlotId.");  return JS_FALSE; }
    if ( NumArguments >= 2 )
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[1], &lba) )
        {
            JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
        }
    if ( NumArguments >= 3 )
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[2], &numSectors) )
        {
            JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
        }
    if ( NumArguments >= 4 )
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[3], &jsData) )
        {
            JS_ReportError(pContext, "Failed to get parameters."); return JS_FALSE;
        }
    vector<UINT32> vData;
    UINT32 dataToTruncate;
    for ( UINT32 i = 0; i< jsData.size(); i++ )
    {
        if ( OK != JavaScriptPtr()->FromJsval((jsData[i]), &dataToTruncate) )
        {
            JS_ReportError(pContext, "data element bad value."); return JS_FALSE;
        }
        vData.push_back(dataToTruncate);
    }
    // if user didn't specify data pattern, generate random pattern
    if ( (vData.size() == 0) )
    {
        JSObject * pReturlwals;
        if ( NumArguments >= 4 )
        {
            if ( OK != JavaScriptPtr()->FromJsval(pArguments[3], &pReturlwals) )
            {
                JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
            }
        }
        for ( UINT32 i = 0 ; i < 128; i++ )
        {   // generate data pattern of 512 Byte
            vData.push_back(DefaultRandom::GetRandom(0, 0xffffffff));
            if ( NumArguments >= 4 )
            {  // if user give zero length array, return random pattern in the array
                if ( OK != (rc = JavaScriptPtr()->SetElement(pReturlwals, i, vData[i])) )
                    JSIF_RETURN( rc );
            }
        }
    }

    UINT32 sectorSize;
    JSIF_CALL_TEST(XhciController, GetSectorSize, slotId, &sectorSize);
    C_CHECK_RC(rc);
    void * pVa;
    UINT32 addrBit = 32;
    C_CHECK_RC(MemoryTracker::AllocBuffer(numSectors * sectorSize, &pVa, true, addrBit, Memory::UC));

    //setup fixed PRD if present
    vector<UINT32> vFixedPrd;
    JsArray jsFixedPrd;
    UINT32 totalByte = 0;
    if ( NumArguments >= 5 )
    {
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[4], &jsFixedPrd) )
        {
            JS_ReportError(pContext, "FixedPRD Arg bad value"); return JS_FALSE;
        }

        for ( UINT32 i = 0; i< jsFixedPrd.size(); i++ )
        {
            if ( OK != JavaScriptPtr()->FromJsval((jsFixedPrd[i]), &dataToTruncate) )
            {
                JS_ReportError(pContext, "data element bad value."); return JS_FALSE;
            }
            totalByte += dataToTruncate;
            vFixedPrd.push_back(dataToTruncate);
        }
        if ( totalByte != numSectors * sectorSize )
        {
            Printf(Tee::PriHigh,
                   "ERROR: Fixed PRD byte length %d doesn't match data size %d",
                   totalByte, numSectors * sectorSize);
            return JS_FALSE;
        }
    }
    // Get total transfer byte size
    totalByte = numSectors * sectorSize;

    // construct fraglist
    MemoryFragment::FRAGLIST fragList;
    if ( 0 == vFixedPrd.size() )
    {   //no fixed PRD specified
        void* pDataBuf = pVa;
        MemoryFragment::FRAGMENT memFrag;
        UINT32 fragSize = ((XhciController*) JS_GetPrivate(pContext, pObject))->GetOptimisedFragLength();
        while ( totalByte )
        {
            UINT32 txSize = (totalByte > fragSize)?fragSize:totalByte;
            memFrag.Address = pDataBuf;
            memFrag.ByteSize = txSize;
            fragList.push_back(memFrag);

            pDataBuf = (void*) ((UINT08*)pDataBuf + txSize);
            totalByte -= txSize;
        }
    }
    else
    {
        MemoryFragment::GenerateFixedFragment(pVa,
                                              0, totalByte,
                                              &fragList, 1, vFixedPrd);
    }

    MemoryFragment::FillPattern(&fragList, &vData, true);

    UINT32 debugMode = ((XhciController*) JS_GetPrivate(pContext, pObject))->GetDebugMode();
    if (XHCI_DEBUG_MODE_USE_SCSI_CMD16 & debugMode)
    {
        JSIF_CALL_TEST(XhciController, CmdWrite16, slotId, lba, numSectors, &fragList);
    }
    else
    {
        JSIF_CALL_TEST(XhciController, CmdWrite10, slotId, lba, numSectors, &fragList);
    }
    MemoryTracker::FreeBuffer(pVa);
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, PrintData, 2, "Print HC data structures")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( (NumArguments != 1) && (NumArguments != 2) )
    {
        JS_ReportError(pContext,"Usage: Xhci.PrintData(Type [0-DCBAA,1-DevContext/Index,2-ExtDev/Index, 3-ERST/Index, 4-StreamContext/Index, 5-All Xfer Rings, 6-Scratchpad Buffer], *Index)");
        return JS_FALSE;
    }
    UINT32 type;
    UINT32 index = 0;

    if ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &type) )
    {
        JS_ReportError(pContext, "Failed to get parameters."); return JS_FALSE;
    }
    if ( NumArguments >= 2 )
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[1], &index) )
        {
            JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
        }

    JSIF_CALL_TEST(XhciController, PrintData, type, index);
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, ReqGetDescriptor, 5, "Issue USB GetDescriptor request")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( ( NumArguments != 4) && (NumArguments != 5) )
    {
        JS_ReportError(pContext,"Usage: Xhci.ReqGetDescriptor(SlotId, Type[1-Device, 2-Config, 3-String], Index, Length, *[Data&])");
        return JS_FALSE;
    }
    UINT32 slotId;
    UINT32 type;
    UINT32 index;
    UINT32 length;
    if ( (OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId))
         || (OK != JavaScriptPtr()->FromJsval(pArguments[1], &type))
         || (OK != JavaScriptPtr()->FromJsval(pArguments[2], &index))
         || (OK != JavaScriptPtr()->FromJsval(pArguments[3], &length)) )
    {
        JS_ReportError(pContext, "Failed to get parameter."); return JS_FALSE;
    }
    JSObject * pReturlwals;
    if ( NumArguments >= 5 )
    {
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[4], &pReturlwals) )
        {
            JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
        }
    }

    void * pVa;
    UINT32 addrBit = 32;
    C_CHECK_RC(MemoryTracker::AllocBuffer(length, &pVa, true, addrBit, Memory::UC));
    JSIF_CALL_TEST(XhciController, ReqGetDescriptor, slotId, (UsbDescriptorType)type, index, pVa, length);
    if ( NumArguments < 5 )
    {
        Memory::Print08(pVa, length, Tee::PriNormal);
    }
    else
    {
        for ( UINT32 i = 0; i < length; i++ )
        {
            JavaScriptPtr()->SetElement(pReturlwals, i, (UINT32) MEM_RD08((UINT08 *)pVa + i));
        }
    }
    MemoryTracker::FreeBuffer(pVa);
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, ReqClearFeature, 4, "Issue USB ClearFeature request")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( NumArguments != 4 )
    {
        JS_ReportError(pContext,"Usage: Xhci.ReqClearFeature(SlotId, Feature[0-Ep_Halt/Func_Susp, 48-U1, 49-U2, 50-LTM], Recipient[0-Dev, 1-Interf, 2-Ep], Index)");
        return JS_FALSE;
    }
    UINT32 slotId;
    UINT32 feature;
    UINT32 recipient;
    UINT32 index;
    if ( (OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId))
         || (OK != JavaScriptPtr()->FromJsval(pArguments[1], &feature))
         || (OK != JavaScriptPtr()->FromJsval(pArguments[2], &recipient))
         || (OK != JavaScriptPtr()->FromJsval(pArguments[3], &index)) )
    {
        JS_ReportError(pContext, "Failed to get parameter."); return JS_FALSE;
    }
    JSIF_CALL_TEST(XhciController, ReqClearFeature, slotId, feature, (UsbRequestRecipient)recipient, index);
    JSIF_RETURN(rc);
}

JS_SMETHOD_LWSTOM(XhciController, ReqGetInterface, 2, "Issue USB GetInterface request")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( NumArguments != 2 )
    {
        JS_ReportError(pContext,"Usage: Xhci.ReqGetInterface(SlotId, Interface)");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    UINT32 slotId;
    UINT32 interface;
    if ( (OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId))
         || (OK != JavaScriptPtr()->FromJsval(pArguments[1], &interface)) )
    {
        JS_ReportError(pContext, "Failed to get parameter."); return JS_FALSE;
    }

    JSIF_CALL_METHOD(XhciController, UINT08, ReqGetInterface, slotId, interface);
    JSIF_RETURN(result);
}

JS_SMETHOD_LWSTOM(XhciController, ReqGetStatus, 3, "Issue USB GetStatus request")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( NumArguments != 3 )
    {
        JS_ReportError(pContext,"Usage: Xhci.ReqGetStatus(SlotId, Recipient[0-Dev, 1-Interf, 2-Ep], Index)");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    UINT32 slotId;
    UINT32 recipient;
    UINT32 index;
    if ( (OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId))
         || (OK != JavaScriptPtr()->FromJsval(pArguments[1], &recipient))
         || (OK != JavaScriptPtr()->FromJsval(pArguments[2], &index)) )
    {
        JS_ReportError(pContext, "Failed to get parameter."); return JS_FALSE;
    }

    JSIF_CALL_METHOD(XhciController, UINT16, ReqGetStatus, slotId, (UsbRequestRecipient)recipient, index);
    JSIF_RETURN(result);
}

JS_STEST_LWSTOM(XhciController, ReqSetConfiguration, 2, "Issue USB SetConfiguration request")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( NumArguments != 2 )
    {
        JS_ReportError(pContext,"Usage: Xhci.ReqSetConfiguration(SlotId, CfgValue)");
        return JS_FALSE;
    }

    UINT32 slotId;
    UINT32 cfgValue;
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[1], &cfgValue) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }

    JSIF_CALL_TEST(XhciController, ReqSetConfiguration, slotId, cfgValue);
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, ReqSetFeature, 6, "Issue USB SetFeature request")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( ( NumArguments != 4 )
         && ( NumArguments != 5 ) )
    {
        JS_ReportError(pContext,"Usage: Xhci.ReqSetFeature(SlotId, Feature, Recipient, Index, SuspendOption)");
        return JS_FALSE;
    }

    UINT32 slotId;
    UINT32 feature;
    UINT32 recipient;
    UINT32 index;
    UINT32 suspend;
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[1], &feature) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[2], &recipient) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[3], &index) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }
    if ( ( NumArguments >= 5 ) && (OK != JavaScriptPtr()->FromJsval(pArguments[4], &suspend)) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }

    if ( NumArguments == 4 )
    {
        JSIF_CALL_TEST(XhciController, ReqSetFeature, slotId, feature, (UsbRequestRecipient)recipient, index);
    }
    else
    {
        JSIF_CALL_TEST(XhciController, ReqSetFeature, slotId, feature, (UsbRequestRecipient)recipient, index, suspend);
    }
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, ReqSetInterface, 3, "Issue USB SetInterface request")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( NumArguments != 3 )
    {
        JS_ReportError(pContext,"Usage: Xhci.ReqSetInterface(SlotId, Alternate, Interface)");
        return JS_FALSE;
    }

    UINT32 slotId;
    UINT32 alternate;
    UINT32 interface;
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[1], &alternate) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[2], &interface) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }

    JSIF_CALL_TEST(XhciController, ReqSetInterface,slotId, alternate, interface);
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, HubReqClearHubFeature, 2, "Issue USB HubClearHubFeature request")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( NumArguments != 2 )
    {
        JS_ReportError(pContext,"Usage: Xhci.HubReqClearHubFeature(SlotId, Feature)");
        return JS_FALSE;
    }

    UINT32 slotId;
    UINT32 feature;
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[1], &feature) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }

    JSIF_CALL_TEST(XhciController, HubReqClearHubFeature, slotId, feature);
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, HubReqClearPortFeature, 4, "Issue USB HubClearPortFeature request")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( ( NumArguments != 3 ) && ( NumArguments != 4 ) )
    {
        JS_ReportError(pContext,"Usage: Xhci.HubReqClearPortFeature(SlotId, Feature, Port, *Indicator)");
        return JS_FALSE;
    }

    UINT32 slotId;
    UINT32 feature;
    UINT32 port;
    UINT32 indicator = 0;
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[1], &feature) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[2], &port) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }
    if ( (NumArguments >=4) && ( OK != JavaScriptPtr()->FromJsval(pArguments[3], &indicator) ) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }

    JSIF_CALL_TEST(XhciController, HubReqClearPortFeature, slotId, feature, port, indicator);
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, HubReqGetHubDescriptor, 2, "Issue USB GetHubStatus request")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( NumArguments != 2 )
    {
        JS_ReportError(pContext,"Usage: Xhci.HubReqGetHubDescriptor(SlotId, Length)");
        return JS_FALSE;
    }
    UINT32 slotId;
    UINT32 length;
    if ( (OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId))
         || (OK != JavaScriptPtr()->FromJsval(pArguments[1], &length)) )
    {
        JS_ReportError(pContext, "Failed to get parameter."); return JS_FALSE;
    }

    void * pVa;
    UINT32 addrBit = 32;
    C_CHECK_RC(MemoryTracker::AllocBuffer(length, &pVa, true, addrBit, Memory::UC));
    JSIF_CALL_TEST(XhciController, HubReqGetHubDescriptor, slotId, pVa, length);
    Memory::Print08(pVa, length, Tee::PriNormal);
    MemoryTracker::FreeBuffer(pVa);
    JSIF_RETURN(rc);
}

JS_SMETHOD_LWSTOM(XhciController, HubReqGetHubStatus, 1, "Issue USB GetHubStatus request")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( NumArguments != 1 )
    {
        JS_ReportError(pContext,"Usage: Xhci.HubReqGetHubStatus(SlotId)");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    UINT32 slotId;
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }

    JSIF_CALL_METHOD(XhciController, UINT32, HubReqGetHubStatus, slotId);
    JSIF_RETURN(result);
}

JS_SMETHOD_LWSTOM(XhciController, HubReqGetPortStatus, 2, "Issue USB HubGetPortStatus request")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( NumArguments != 2 )
    {
        JS_ReportError(pContext,"Usage: Xhci.HubReqGetPortStatus(SlotId, Port)");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    UINT32 slotId;
    UINT32 port;
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[1], &port) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }

    JSIF_CALL_METHOD(XhciController, UINT32, HubReqGetPortStatus, slotId, port);
    JSIF_RETURN(result);
}

JS_SMETHOD_LWSTOM(XhciController, HubReqPortErrorCount, 2, "Issue USB HubPortErrorCount request")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( NumArguments != 2 )
    {
        JS_ReportError(pContext,"Usage: Xhci.HubReqPortErrorCount(SlotId, Port)");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    UINT32 slotId;
    UINT32 port;
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[1], &port) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }

    JSIF_CALL_METHOD(XhciController, UINT16, HubReqPortErrorCount, slotId, port);
    JSIF_RETURN(result);
}

JS_STEST_LWSTOM(XhciController, HubReqSetHubFeature, 2, "Issue USB HubSetHubFeature request")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( NumArguments < 2 ||  NumArguments > 4 )
    {
        JS_ReportError(pContext,"Usage: Xhci.HubReqSetHubFeature(SlotId, Feature, *Index, *Length)");
        return JS_FALSE;
    }

    UINT32 slotId;
    UINT32 feature;
    UINT32 index = 0;
    UINT32 length = 0;
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[1], &feature) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }
    if ( NumArguments > 2 )
    {
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[2], &index) )
        {
            JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
        }

    }
    if ( NumArguments > 3 )
    {
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[3], &length) )
        {
            JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
        }

    }
    JSIF_CALL_TEST(XhciController, HubReqSetHubFeature, slotId, feature, index, length);
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, HubReqSetHubDepth, 2, "Issue USB HubSetHubDepth request")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( NumArguments != 2 )
    {
        JS_ReportError(pContext,"Usage: Xhci.HubReqSetHubDepth(SlotId, Depth)");
        return JS_FALSE;
    }

    UINT32 slotId;
    UINT32 depth;
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[1], &depth) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }

    JSIF_CALL_TEST(XhciController, HubReqSetHubDepth, slotId, depth);
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, HubReqSetPortFeature, 4, "Issue USB HubSetPortFeature request")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( ( NumArguments != 3 ) && ( NumArguments != 4 ) )
    {
        JS_ReportError(pContext,"Usage: Xhci.HubReqSetPortFeature(SlotId, Feature, Port, *Index)");
        return JS_FALSE;
    }

    UINT32 slotId;
    UINT32 feature;
    UINT32 port;
    UINT32 index = 0;
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[1], &feature) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[2], &port) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }
    if ( ( NumArguments >= 4 ) && (OK != JavaScriptPtr()->FromJsval(pArguments[3], &index)) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }

    JSIF_CALL_TEST(XhciController, HubReqSetPortFeature, slotId, feature, port, index);
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, SetupStream, 2, "Setup Stream layout on runtime")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( ( NumArguments != 1 )
         && ( NumArguments != 5 ) )
    {
        JS_ReportError(pContext,"Usage: Xhci.SetupStream([Size]) or Xhci.SetupStream(SlotId, Endpoint, IsDataOut, Index, Size)");
        return JS_FALSE;
    }

    JsArray jsData;
    UINT32 slotId, endpoint;
    bool isDataOut;
    UINT32 index;
    UINT32 size;
    if ( NumArguments == 1 )
    {
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &jsData) )
        {
            JS_ReportError(pContext, "Failed to get parameter."); return JS_FALSE;
        }
        UINT32 dataToTruncate;
        vector<UINT32> vData;
        for ( UINT32 i = 0; i< jsData.size(); i++ )
        {
            if ( OK != JavaScriptPtr()->FromJsval((jsData[i]), &dataToTruncate) )
            {
                JS_ReportError(pContext, "data element bad value."); return JS_FALSE;
            }
            vData.push_back(dataToTruncate);
        }
        JSIF_CALL_TEST(XhciController, SetStreamLayout, &vData);
    }
    else
    {
        if ( (OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId))
             || (OK != JavaScriptPtr()->FromJsval(pArguments[1], &endpoint))
             || (OK != JavaScriptPtr()->FromJsval(pArguments[2], &isDataOut))
             || (OK != JavaScriptPtr()->FromJsval(pArguments[3], &index))
             || (OK != JavaScriptPtr()->FromJsval(pArguments[4], &size)) )
        {
            JS_ReportError(pContext, "Failed to get parameter."); return JS_FALSE;
        }
        JSIF_CALL_TEST(XhciController, SetupStreamSecArray, slotId, endpoint, isDataOut, index, size);
    }
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, InitStream, 3, "Initialize Stream")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( NumArguments != 4 )
    {
        JS_ReportError(pContext,"Usage: Xhci.InitStream(SlotId, Endpoint, IsDataOut, *[AddrLo, AddrHi]&)");
        return JS_FALSE;
    }

    JsArray jsData;
    UINT32 slotId, endpoint;
    bool isDataOut;
    if ( (OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId))
         || (OK != JavaScriptPtr()->FromJsval(pArguments[1], &endpoint))
         || (OK != JavaScriptPtr()->FromJsval(pArguments[2], &isDataOut)))
    {
        JS_ReportError(pContext, "Failed to get parameter."); return JS_FALSE;
    }
    PHYSADDR pPhyBase;
    JSObject * pReturlwals;
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[3], &pReturlwals) )
    {
        JS_ReportError(pContext, "Failed to get parameter."); return JS_FALSE;
    }

    JSIF_CALL_TEST(XhciController, InitStream, slotId, endpoint, isDataOut, &pPhyBase);
    C_CHECK_RC(rc);
    Printf(Tee::PriDebug, "BAR of Stream Context is 0x%llx", pPhyBase);
    UINT32 tmp = pPhyBase;
    JavaScriptPtr()->SetElement(pReturlwals, 0, tmp);
    pPhyBase >>= 32;
    tmp = pPhyBase;
    JavaScriptPtr()->SetElement(pReturlwals, 1, tmp);

    JSIF_RETURN(rc);
}
/*
C_(Xhci_SaveFw)
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( ( NumArguments != 0 ) &&( NumArguments != 1 ) &&( NumArguments != 2 ) )
    {
        JS_ReportError(pContext,"Usage: Xhci.SaveFW(*InitTag, *ByteSize)");
        return JS_FALSE;
    }

    UINT32 initTag = 0;
    UINT32 byteSize = 1024;
    if ( ( NumArguments >= 1 )
         && ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &initTag) ) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }
    if ( ( NumArguments >= 2 )
         && ( OK != JavaScriptPtr()->FromJsval(pArguments[1], &byteSize) ) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }

    XhciController* pDev = NULL;
    C_CHECK_RC(XhciMgr::Mgr()->GetLwrrent((McpDev**)&pDev));
    C_CHECK_RC(pDev->SaveFW(initTag, byteSize));

    JSIF_RETURN(rc);
}
*/

JSIF_TEST_1(XhciController, ClearRing, UINT32, RingId, "Clear specified Ring to initial state")
JS_STEST_LWSTOM(XhciController, PrintRing, 1, "Print content of specific Ring")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( ( NumArguments != 0)
         && (NumArguments != 1 ) )
    {
        JS_ReportError(pContext,"Usage: Xhci.PrintRing(RingId*)");
        return JS_FALSE;
    }

    if ( NumArguments == 0 )
    {
        JSIF_CALL_TEST(XhciController, PrintRingList);
    }
    else
    {
        UINT32 ringId;
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &ringId) )
        {
            JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
        }
        JSIF_CALL_TEST(XhciController, PrintRing, ringId);
    }

    JSIF_RETURN(rc);
}

JS_SMETHOD_LWSTOM(XhciController, DevRegRd, 2, "Read register of configurable USB device")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( NumArguments != 2 )
    {
        JS_ReportError(pContext,"Usage: Xhci.DevRegRd(SlotId, Offset)");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    UINT32 slotId, offset;
    if ( ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId) )
         || ( OK != JavaScriptPtr()->FromJsval(pArguments[1], &offset) ) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }

    JSIF_CALL_METHOD(XhciController, UINT64, DevRegRd, slotId, offset);
    JSIF_METHOD_CHECK_RC(XhciController, DevRegRd);

    JsArray retVal(2, jsval(0));
    UINT32 tmp = result;
    JavaScriptPtr()->ToJsval(tmp, &retVal[0]);
    result >>= 32;
    tmp = result;
    JavaScriptPtr()->ToJsval(tmp, &retVal[1]);
    if (OK != JavaScriptPtr()->ToJsval(&retVal, pReturlwalue))
    {
        JS_ReportError(pContext, "Error in DevRegRd()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

JS_STEST_LWSTOM(XhciController, DevRegWr, 3, "Write register of configurable USB device")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( NumArguments != 3 )
    {
        JS_ReportError(pContext,"Usage: Xhci.DevRegWr(SlotId, Offset, [Data32Lsb, Data32Msb])");
        return JS_FALSE;
    }

    UINT32 slotId, offset;
    JsArray jsData32;
    UINT64 data64;
    if ( ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId) )
         || (OK != JavaScriptPtr()->FromJsval(pArguments[1], &offset))
         || (OK != JavaScriptPtr()->FromJsval(pArguments[2], &jsData32)) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }
    UINT32 tmp;
    if ( OK != JavaScriptPtr()->FromJsval((jsData32[1]), &tmp) )
    { JS_ReportError(pContext, "data element bad value."); return JS_FALSE; }
    data64 = tmp;
    data64 <<= 32;
    if ( OK != JavaScriptPtr()->FromJsval((jsData32[0]), &tmp) )
    { JS_ReportError(pContext, "data element bad value."); return JS_FALSE; }
    data64 |= tmp;
    //Printf(Tee::PriHigh, "Get value 0x%08llx", data64);

    JSIF_CALL_TEST(XhciController, DevRegWr, slotId, offset, data64);
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, DataIn, 3, "Read data from configurable USB device")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( (NumArguments < 3) || (NumArguments > 5) )
    {
        JS_ReportError(pContext,"Usage: Xhci.DataIn(SlotId, Endpoint, Length, *[DataArray08]&, *[FixedPRD])"); return JS_FALSE;
    }
    UINT32 slotId, endpoint, length;
    JSObject * pReturlwals;
    if ( ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId) )
         || ( OK != JavaScriptPtr()->FromJsval(pArguments[1], &endpoint) )
         || ( OK != JavaScriptPtr()->FromJsval(pArguments[2], &length) ) )
    {
        JS_ReportError(pContext, "Failed to get parameter"); return JS_FALSE;
    }
    if ( NumArguments >= 4 )
    {
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[3], &pReturlwals) )
        {
            JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
        }
    }
    //setup fixed PRD if present
    vector<UINT32> vFixedPrd;
    JsArray jsFixedPrd;
    UINT32 dataToTruncate;
    UINT32 totalByte = 0;
    if ( NumArguments >= 5 )
    {
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[4], &jsFixedPrd) )
        {
            JS_ReportError(pContext, "FixedPRD Arg bad value"); return JS_FALSE;
        }

        for ( UINT32 i = 0; i< jsFixedPrd.size(); i++ )
        {
            if ( OK != JavaScriptPtr()->FromJsval((jsFixedPrd[i]), &dataToTruncate) )
            {
                JS_ReportError(pContext, "data element bad value."); return JS_FALSE;
            }
            totalByte += dataToTruncate;
            vFixedPrd.push_back(dataToTruncate);
        }
        if ( totalByte != length )
        {
            Printf(Tee::PriHigh,
                   "ERROR: Fixed PRD byte length %d doesn't match data size %d",
                   totalByte, length);
            return JS_FALSE;
        }
    }
    // Get total transfer byte size
    totalByte = length;

    void * pVa;
    UINT32 pageSize = 4096;
    UINT32 addrBit = 32;
    C_CHECK_RC(MemoryTracker::AllocBufferAligned(length, &pVa, pageSize, addrBit, Memory::UC));
    Memory::Fill08(pVa, 0xff, length);

    // construct fraglist
    MemoryFragment::FRAGLIST fragList;
    if ( 0 == vFixedPrd.size() )
    {   //no fixed PRD specified
        void* pDataBuf = pVa;
        MemoryFragment::FRAGMENT memFrag;
        UINT32 fragSize = 1024;
        JSIF_CALL_TEST(XhciController, GetEpMps, slotId, endpoint, false, &fragSize);
        C_CHECK_RC_CLEANUP(rc);
        while ( totalByte )
        {
            UINT32 txSize = (totalByte > fragSize)?fragSize:totalByte;
            memFrag.Address = pDataBuf;
            memFrag.ByteSize = txSize;
            fragList.push_back(memFrag);

            pDataBuf = (void*) ((UINT08*)pDataBuf + txSize);
            totalByte -= txSize;
        }
    }
    else
    {
        MemoryFragment::GenerateFixedFragment(pVa,
                                              0, totalByte,
                                              &fragList, 1, vFixedPrd);
    }

    JSIF_CALL_TEST(XhciController, DataIn, slotId, endpoint, &fragList);
    if ( rc == OK  )
    {
        if ( NumArguments >= 4 )
        {
            totalByte = 0;
            for ( UINT32 i = 0; i < fragList.size(); i++ )
            {
                UINT08 * pTmp = (UINT08*) fragList[i].Address;
                for ( UINT32 j = 0; j < fragList[i].ByteSize; j++ )
                {
                    JavaScriptPtr()->SetElement(pReturlwals, totalByte, MEM_RD08(pTmp+j));
                    totalByte++;
                }
            }
        }
        else
            MemoryFragment::Print32(&fragList);
    }
Cleanup:
    MemoryTracker::FreeBuffer(pVa);
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, DataOut, 3, "Write data to configurable USB device")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( NumArguments < 4|| NumArguments > 5 )
    {
        JS_ReportError(pContext,"Usage: Xhci.DataOut(SlotId, Endpoint, Length, [Pattern08], *[FixedPRD])"); return JS_FALSE;
    }
    UINT32 slotId, endpoint, length;
    JsArray  jsData;
    if ( ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId) )
         || ( OK != JavaScriptPtr()->FromJsval(pArguments[1], &endpoint) )
         || ( OK != JavaScriptPtr()->FromJsval(pArguments[2], &length) )
         || ( OK != JavaScriptPtr()->FromJsval(pArguments[3], &jsData) ) )
    {
        JS_ReportError(pContext, "Failed to get parameter"); return JS_FALSE;
    }
    //setup fixed PRD if present
    UINT32 dataToTruncate;
    vector<UINT32> vFixedPrd;
    JsArray jsFixedPrd;
    UINT32 totalByte = 0;
    if ( NumArguments >= 5 )
    {
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[4], &jsFixedPrd) )
        {
            JS_ReportError(pContext, "FixedPRD Arg bad value"); return JS_FALSE;
        }

        for ( UINT32 i = 0; i< jsFixedPrd.size(); i++ )
        {
            if ( OK != JavaScriptPtr()->FromJsval((jsFixedPrd[i]), &dataToTruncate) )
            {
                JS_ReportError(pContext, "data element bad value."); return JS_FALSE;
            }
            totalByte += dataToTruncate;
            vFixedPrd.push_back(dataToTruncate);
        }
        if ( totalByte != length )
        {
            Printf(Tee::PriHigh,
                   "ERROR: Fixed PRD byte length %d doesn't match data size %d",
                   totalByte, length);
            return JS_FALSE;
        }
    }
    // Get total transfer byte size
    totalByte = length;

    // retrieve the data pattern
    vector<UINT08> vData;
    UINT08 temp;
    for ( UINT32 i = 0; i< jsData.size(); i++ )
    {
        if ( OK != JavaScriptPtr()->FromJsval((jsData[i]), &dataToTruncate) )
        {
            JS_ReportError(pContext, "data element bad value."); return JS_FALSE;
        }
        temp = dataToTruncate;
        vData.push_back(temp);
    }

    void * pVa;
    UINT32 pageSize = 4096;
    UINT32 addrBit = 32;
    C_CHECK_RC(MemoryTracker::AllocBufferAligned(length, &pVa, pageSize, addrBit, Memory::UC));

    // construct fraglist
    MemoryFragment::FRAGLIST fragList;
    if ( 0 == vFixedPrd.size() )
    {   //no fixed PRD specified
        void* pDataBuf = pVa;
        MemoryFragment::FRAGMENT memFrag;
        UINT32 fragSize = 1024;
        JSIF_CALL_TEST(XhciController, GetEpMps, slotId, endpoint, true, &fragSize);
        C_CHECK_RC(rc);
        while ( totalByte )
        {
            UINT32 txSize = (totalByte > fragSize)?fragSize:totalByte;
            memFrag.Address = pDataBuf;
            memFrag.ByteSize = txSize;
            fragList.push_back(memFrag);

            pDataBuf = (void*) ((UINT08*)pDataBuf + txSize);
            totalByte -= txSize;
        }
    }
    else
    {
        MemoryFragment::GenerateFixedFragment(pVa,
                                              0, totalByte,
                                              &fragList, 1, vFixedPrd);
    }
    MemoryFragment::FillPattern(&fragList, &vData, true);
    JSIF_CALL_TEST(XhciController, DataOut, slotId, endpoint, &fragList);
    MemoryTracker::FreeBuffer(pVa);
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, DataDone, 3, "Retrieve the data associated with specified Transfer Ring and relase the memory")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( NumArguments < 3 || NumArguments > 6 )
    {
        JS_ReportError(pContext,"Usage: Xhci.DataDone(SlotId, Endpoint, IsDataOut, *StreamId, *[DataArray08]&, *NumTd)"); return JS_FALSE;
    }
    UINT32 slotId, endpoint;
    bool isDataOut;
    UINT32 streamId = 0;
    JSObject * pReturlwals;
    UINT32 numTd = 0;
    if ( ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId) )
         || ( OK != JavaScriptPtr()->FromJsval(pArguments[1], &endpoint) )
         || ( OK != JavaScriptPtr()->FromJsval(pArguments[2], &isDataOut) ) )
    {
        JS_ReportError(pContext, "Failed to get parameter"); return JS_FALSE;
    }
    if ( NumArguments >= 4 )
    {
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[3], &streamId) )
        { JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE; }
    }
    if ( NumArguments >= 5 )
    {
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[4], &pReturlwals) )
        { JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE; }
    }
    if ( NumArguments >= 6 )
    {
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[5], &numTd) )
        { JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE; }
    }

    MemoryFragment::FRAGLIST myFragList;
    JSIF_CALL_TEST(XhciController, DataRetrieve, slotId, endpoint, isDataOut, &myFragList, streamId, numTd);
    C_CHECK_RC(rc);
    UINT32 totalByte;
    if ( NumArguments >= 5 )
    {
        totalByte = 0;
        for ( UINT32 i = 0; i < myFragList.size(); i++ )
        {
            UINT08 * pTmp = (UINT08*) myFragList[i].Address;
            for ( UINT32 j = 0; j < myFragList[i].ByteSize; j++ )
            {
                JavaScriptPtr()->SetElement(pReturlwals, totalByte, MEM_RD08(pTmp+j));
                totalByte++;
            }
        }
    }
    else
        MemoryFragment::Print32(&myFragList);

    // release the memory if data has been retrieved
    if ( NumArguments >= 5 )
    {   // if user collect data into array, we delete the buffer
        JSIF_CALL_TEST(XhciController, DataRelease, slotId, endpoint, isDataOut, streamId, numTd);
        if (numTd)
        {
            Printf(Tee::PriNormal, "%d TDs deleted \n", numTd);
        }
        else
        {
            Printf(Tee::PriNormal, "All buffer deleted \n");
        }
    }
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, InsertTd, 3, "Insert TRBs into specified Transfer Ring")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( NumArguments < 4 || NumArguments > 7 )
    {
        JS_ReportError(pContext,"Usage: Xhci.InsertTd(SlotId, Endpoint, IsDataOut, [Trb Sizes], [Pattern08], *IsIsoch, *StreamId)"); return JS_FALSE;
    }
    UINT32 slotId, endpoint;
    bool isDataOut;
    JsArray  jsData, jsTrbSize;
    // JSObject * pReturlwals;
    if ( ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId) )
         || ( OK != JavaScriptPtr()->FromJsval(pArguments[1], &endpoint) )
         || ( OK != JavaScriptPtr()->FromJsval(pArguments[2], &isDataOut) )
         || ( OK != JavaScriptPtr()->FromJsval(pArguments[3], &jsTrbSize) ) )
    {
        JS_ReportError(pContext, "Failed to get parameter"); return JS_FALSE;
    }
    if ( isDataOut )
    {   // retrieve the Data Pattern
        if ( NumArguments < 5 )
        { JS_ReportError(pContext, "Data Pattern parameter missed for Data Out TD"); return JS_FALSE; }
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[4], &jsData) )
        { JS_ReportError(pContext, "Failed to get parameter"); return JS_FALSE; }
        if ( 0 == jsData.size())
        { JS_ReportError(pContext, "Data Pattern parameter empty for Data Out TD"); return JS_FALSE;}
    }
    bool isIsoch = false;
    if ( NumArguments >= 6 )
    {
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[5], &isIsoch) )
        { JS_ReportError(pContext, "Failed to get parameter"); return JS_FALSE; }
    }
    UINT32 streamId = 0;
    if ( NumArguments >= 7 )
    {
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[6], &streamId) )
        { JS_ReportError(pContext, "Failed to get parameter StreamId"); return JS_FALSE; }
    }
    //setup buffer sizes of TRBs
    UINT32 dataToTruncate;
    vector<UINT32> vTrbSizes;
    UINT32 totalByte = 0;
    Printf(Tee::PriNormal, "Build Normal TRBs with buffer size: \n");
    for ( UINT32 i = 0; i< jsTrbSize.size(); i++ )
    {
        if ( OK != JavaScriptPtr()->FromJsval((jsTrbSize[i]), &dataToTruncate) )
        { JS_ReportError(pContext, "data element bad value."); return JS_FALSE; }
        totalByte += dataToTruncate;
        vTrbSizes.push_back(dataToTruncate);
        Printf(Tee::PriNormal, " %d,", dataToTruncate);
    }
    Printf(Tee::PriNormal, " Total Byte %d \n", totalByte);

    void * pVa;
    // if there's any dummy buffer exists, use it. otherwise allocate new buffer
    bool isDummyBuffer = true;
    JSIF_CALL_TEST(XhciController, GetDummyBuf, &pVa, totalByte);
    C_CHECK_RC(rc);
    if (!pVa)
    {
        isDummyBuffer = false;
        UINT32 addrBit = 32;
        C_CHECK_RC(MemoryTracker::AllocBufferAligned(totalByte, &pVa, 64, addrBit, Memory::UC));
        Memory::Fill08(pVa, 0, totalByte);
    }

    // construct fraglist
    MemoryFragment::FRAGLIST fragList;
    MemoryFragment::GenerateFixedFragment(pVa,
                                          0, totalByte,
                                          &fragList, 1, vTrbSizes);

    // setup the data pattern if Data Out
    if (isDataOut)
    {
        vector<UINT08> vData;
        UINT08 temp;
        for ( UINT32 i = 0; i< jsData.size(); i++ )
        {
            if ( OK != JavaScriptPtr()->FromJsval((jsData[i]), &dataToTruncate) )
            { JS_ReportError(pContext, "data element bad value."); return JS_FALSE; }
            temp = dataToTruncate;
            vData.push_back(temp);
        }
        MemoryFragment::FillPattern(&fragList, &vData, true);
    }
    // if using dummy buffer, don't push the buffer address for later release
    JSIF_CALL_TEST(XhciController, SetupTd, slotId, endpoint, isDataOut, &fragList, isIsoch, false, streamId, isDummyBuffer?NULL:pVa);
    if (OK != rc)
    {
        MemoryTracker::FreeBuffer(pVa);
    }
    JSIF_RETURN(rc);
}

JS_SMETHOD_LWSTOM(XhciController, GetCmdCbw, 4, "Return the CBW for SCSI commands MODS generated")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( NumArguments != 4 )
    {
        JS_ReportError(pContext,"Usage: Xhci.ReadSectorCmd(SlotId, Lba, NumSectors, Cmd[0-Write10|1-Read10])");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    UINT32 slotId, lba, numSectors, cmd;
    if ( ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId) )
         || ( OK != JavaScriptPtr()->FromJsval(pArguments[1], &lba) )
         || ( OK != JavaScriptPtr()->FromJsval(pArguments[2], &numSectors) )
         || ( OK != JavaScriptPtr()->FromJsval(pArguments[3], &cmd) ) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }
    if ( 0 != cmd && 1 != cmd )
    {
        JS_ReportError(pContext, "Only Read10 and Write10 command are supported at this point");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    if ( 0 == cmd)
    {
        JSIF_CALL_METHOD(XhciController, vector<UINT08>, CmdWrite10, slotId, lba, numSectors, NULL);
        JSIF_RETURN(result);
    }
    else if ( 1 == cmd)
    {
        JSIF_CALL_METHOD(XhciController, vector<UINT08>, CmdRead10, slotId, lba, numSectors, NULL);
        JSIF_RETURN(result);
    }
    else
    {
        Printf(Tee::PriError, "Invalid CMD %d \n", cmd);
    }
    JSIF_RETURN(OK);
}

JS_SMETHOD_LWSTOM(XhciController, Wr64Bit, 1, "64bit integer bit field operation")
{   // this is the function to handle 64 integer bit manipulation since Javascript handles only 52bit
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    if ( NumArguments != 3 && NumArguments != 4)
    {
        JS_ReportError(pContext,"Usage: Xhci.Wr64Bit([Data32Lsb, Data32Msb], LeftBit, RightBit, Value*)");
        *pReturlwalue = JSVAL_NULL; return JS_FALSE;
    }

    UINT32 data32Lsb, data32Msb, leftBit, rightBit, value;
    JsArray  jsData;
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &jsData) )
    { JS_ReportError(pContext, "Failed to get data Array in form [Data32Lsb, data32Msb]."); return JS_FALSE; }
    if ( 2 != jsData.size())
    { JS_ReportError(pContext, "Data Array should has 2 elements"); return JS_FALSE; }

    if ( OK != JavaScriptPtr()->FromJsval((jsData[0]), &data32Lsb) )
    { JS_ReportError(pContext, "data element bad value."); return JS_FALSE; }
    if ( OK != JavaScriptPtr()->FromJsval((jsData[1]), &data32Msb) )
    { JS_ReportError(pContext, "data element bad value."); return JS_FALSE; }

    if ( ( OK != JavaScriptPtr()->FromJsval(pArguments[1], &leftBit) )
         || ( OK != JavaScriptPtr()->FromJsval(pArguments[2], &rightBit) ) )
    {
        JS_ReportError(pContext, "Js Arg bad value");
        *pReturlwalue = JSVAL_NULL; return JS_FALSE;
    }
    UINT32 bitLength = leftBit - rightBit + 1;
    if (leftBit > 63 || rightBit > 63 || bitLength > 32 || rightBit > leftBit)
    {
        JS_ReportError(pContext, "Invalid Bit range[%u - %u]", leftBit, rightBit);
        *pReturlwalue = JSVAL_NULL; return JS_FALSE;
    }
    UINT64 data64 = data32Msb;
    data64 <<= 32;
    data64 |= data32Lsb;
    Printf(Tee::PriLow, "Original Data64 = 0x%0llx \n", data64);
    if (NumArguments >= 4)
    {   // user wants to set value
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[3], &value) )
        {
            JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
            *pReturlwalue = JSVAL_NULL; return JS_FALSE;
        }
        UINT64 tmp64 = ~0;
        tmp64 <<= 64 - leftBit - 1;
        tmp64 >>= 64 - leftBit - 1; // get rid of the left don't-care bit
        tmp64 >>= rightBit;
        if ( value & (~tmp64) )
        {
            JS_ReportError(pContext, "Data overflow, 0x%x could not be fit in %u bits", value, bitLength);
            *pReturlwalue = JSVAL_NULL; return JS_FALSE;
        }
        tmp64 <<= rightBit;         // get rid of the right don't-care bit
        Printf(Tee::PriLow, "Bit Mask = 0x%0llx \n", tmp64);
        data64 &= ~tmp64;    // clear the old bit
        // use tmp64 to switch 64 bit value
        tmp64 = value;
        tmp64 <<= rightBit;
        data64 |= tmp64;
        Printf(Tee::PriLow, "Data64 = 0x%0llx \n", data64);

        value = data64;
        JsArray retVal(2, jsval(0));
        JavaScriptPtr()->ToJsval(value, &retVal[0]);
        Printf(Tee::PriLow, "DataLsb = 0x%08x \n", value);
        data64 >>= 32;
        value = data64;
        JavaScriptPtr()->ToJsval(value, &retVal[1]);
        Printf(Tee::PriLow, "DataMsb = 0x%08x \n", value);
        if (OK != JavaScriptPtr()->ToJsval(&retVal, pReturlwalue))
        {
            JS_ReportError(pContext, "Error in data colwersion");
            *pReturlwalue = JSVAL_NULL; return JS_FALSE;
        }
    }
    else
    { // user wants to retrieve value
        data64 <<= 64 - leftBit - 1;
        data64 >>= 64 - leftBit - 1;
        data64 >>= rightBit;
        value = data64;
        Printf(Tee::PriLow, "return value = 0x%x \n", value);
        if (OK != JavaScriptPtr()->ToJsval(value, pReturlwalue))
        {
            JS_ReportError(pContext, "Error in data colwersion");
            *pReturlwalue = JSVAL_NULL; return JS_FALSE;
        }
    }
    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(XhciController, CheckXferEvent, 5, "Check if there's Xfer Event for specific Endpoint arrived")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if (  NumArguments != 5 )
    {
        JS_ReportError(pContext,"Usage: Xhci.CheckXferEvent(EventIndex, SlotId, Endpoint, IsDataOut, [TRB&])");
        return JS_FALSE;
    }
    UINT32 eventIndex;
    UINT32 slotId;
    UINT32 endpoint;
    bool isDataOut;
    JSObject * pReturlwals;
    if ( (OK != JavaScriptPtr()->FromJsval(pArguments[0], &eventIndex))
         || (OK != JavaScriptPtr()->FromJsval(pArguments[1], &slotId))
         || (OK != JavaScriptPtr()->FromJsval(pArguments[2], &endpoint))
         || (OK != JavaScriptPtr()->FromJsval(pArguments[3], &isDataOut))
         || (OK != JavaScriptPtr()->FromJsval(pArguments[4], &pReturlwals)) )
    {
        JS_ReportError(pContext, "Failed to get parameter."); return JS_FALSE;
    }

    bool hasEvent = false;
    XhciTrb myTrb;
    hasEvent = ((XhciController*) JS_GetPrivate(pContext, pObject))->
        CheckXferEvent(eventIndex, slotId, endpoint, isDataOut, &myTrb);
    if (hasEvent)
    {
        if ( OK != (rc = JavaScriptPtr()->SetElement(pReturlwals, 0, myTrb.DW0)) )
            return JS_FALSE;
        if ( OK != (rc = JavaScriptPtr()->SetElement(pReturlwals, 1, myTrb.DW1)) )
            return JS_FALSE;
        if ( OK != (rc = JavaScriptPtr()->SetElement(pReturlwals, 2, myTrb.DW2)) )
            return JS_FALSE;
        if ( OK != (rc = JavaScriptPtr()->SetElement(pReturlwals, 3, myTrb.DW3)) )
            return JS_FALSE;
    }

    if ( OK != JavaScriptPtr()->ToJsval(hasEvent, pReturlwalue) )
    {
        JS_ReportError(pContext, "Error in data colwersion");
        *pReturlwalue = JSVAL_NULL; return JS_FALSE;
    }
    return JS_TRUE;
}

JS_STEST_LWSTOM(XhciController, DumpRing, 3, "Dump the Ring containing the specified TRB")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( ( NumArguments != 1 )
         && (NumArguments != 3 ) )
    {
        JS_ReportError(pContext,"Usage: Xhci.DumpRing(TrbPhysAddr, *NumTrb, *IsEventRing)");
        return JS_FALSE;
    }

    UINT64 trbAddress;
    UINT32 numTrb = 0;
    bool isEventRing = false;
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &trbAddress) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }
    if ( NumArguments == 3 )
    {
        if ( (OK != JavaScriptPtr()->FromJsval(pArguments[1], &numTrb))
            ||  (OK != JavaScriptPtr()->FromJsval(pArguments[2], &isEventRing)) )
        {
            JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
        }
    }

    JSIF_CALL_TEST(XhciController, DumpRing, (PHYSADDR)trbAddress, numTrb, isEventRing);
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, SendIsoCmd, 2, "Issue vendor defined Isoch Commands (0-start,1-stop,2-query,3-reset) ")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( NumArguments != 3 )
    {
        JS_ReportError(pContext,"Usage: Xhci.SetAddrs(SlotId, Cmd, *[Param0, Param1, ...])\n"
        "       Xhci.SetAddrs(SlotId, 0[start], *[Ep, SIA, FrameId, NumTD])\n"
        "       Xhci.SetAddrs(SlotId, 1[stop], *[Ep, FrameId, SIA])\n"
        "       Xhci.SetAddrs(SlotId, 2[query], *[Ep])\n"
        "       Xhci.SetAddrs(SlotId, 3[reset], *[Ep])\n");
        return JS_FALSE;
    }
    UINT08 slotId = 0;
    UINT32 cmdCode = 0;
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &slotId) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }
    if ( OK != JavaScriptPtr()->FromJsval(pArguments[1], &cmdCode) )
    {
        JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
    }

    JsArray jsData;
    UINT32 dataToTruncate;
    vector<UINT32> vData;
    if ( NumArguments > 2 )
    {
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[2], &jsData) )
        {
            JS_ReportError(pContext, "Failed to get parameter."); return JS_FALSE;
        }

        for ( UINT32 i = 0; i< jsData.size(); i++ )
        {
            if ( OK != JavaScriptPtr()->FromJsval((jsData[i]), &dataToTruncate) )
            {
                JS_ReportError(pContext, "data element bad value."); return JS_FALSE;
            }
            vData.push_back(dataToTruncate);
        }
    }

    JSIF_CALL_TEST(XhciController, SendIsoCmd, slotId, cmdCode, &vData);
    JSIF_RETURN(rc);
}

JSIF_TEST_0(XhciController, HandleFwMsg, "Handle msg from firmware")
//JSIF_TEST_2(XhciController, EnablePortTestMode, UINT32, PortIndex, UINT08, TestMode, "Enable test mode on specified port")
JS_STEST_LWSTOM(XhciController, EnablePortTestMode, 3, "Enable test mode on specified port")
{
    JSIF_INIT_VAR(XhciController, EnablePortTestMode, 2, 3, Port, TestMode, *HubPath);
    JSIF_GET_ARG(0, UINT08, Port);
    JSIF_GET_ARG(1, UINT08, TestMode);
    JSIF_GET_ARG_OPT(2, UINT32, HubPath, 0);

    // TestMode > 5 on hub device is supported in EnablePortTestMode()
    if (0 == HubPath || TestMode > 5)
    {
        JSIF_CALL_TEST(XhciController, EnablePortTestMode, Port, TestMode, HubPath);
    }
    else
    {
        JSIF_CALL_TEST(XhciController, EnableHubPortTestMode, Port, TestMode, HubPath);
    }
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, DisablePortTestMode, 2, "Disable test mode")
{
    JSIF_INIT_VAR(XhciController, DisablePortTestMode, 0, 2, *Port, *HubPath);
    JSIF_GET_ARG_OPT(0, UINT32, Port, 0);
    JSIF_GET_ARG_OPT(1, UINT32, HubPath, 0);

    if (0 == HubPath)
    {
        JSIF_CALL_TEST(XhciController, DisablePortTestMode);
    }
    else
    {
        JSIF_CALL_TEST(XhciController, DisableHubPortTestMode, Port, HubPath);
    }
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XhciController, SaveFwLog, 1, "Save firmware log in a file fw_log.txt if user doesnt specify file name")
{
    JSIF_INIT_VAR(XhciController, SaveFwLog, 0, 1, *FileName);
    JSIF_GET_ARG_OPT(0, string, FileName, "fw_log.txt");
    JSIF_CALL_TEST(XhciController, SaveFwLog, FileName);
    JSIF_RETURN(rc);
}

JSIF_TEST_0(XhciController, PrintFwStatus, "Print the FW run-time status")
JSIF_TEST_0(XhciController, SetPciReservedSize, "Set Byte size of PCI CFG space which resides before MMIO space")
JSIF_TEST_1(XhciController, DisablePorts, vector<UINT32>, Ports, "Disable port[s] in the array of ports")
JSIF_TEST_2(XhciController, SetLowPowerMode, UINT32, Port, UINT32, LowPwrMode, "set low power mode")
JSIF_TEST_1(XhciController, Usb2ResumeDev,   UINT32, Port, "resume USB2 device form suspend mode")
