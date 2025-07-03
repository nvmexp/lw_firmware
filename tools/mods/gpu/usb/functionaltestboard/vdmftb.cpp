/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/usb/functionaltestboard/vdmftb.h"

#include "core/include/tasker.h"
#include "core/include/xp.h"
#include "gpu/include/testdevice.h"
#include "core/include/bitfield.h"

namespace
{
    struct FieldDesc
    {
        UINT32 offset;
        UINT32 size;
    };

    constexpr UINT08    VdmHeaderOffset     = 32;
    constexpr UINT08    VdmHeaderBodySize   = 32;         // 32 bits each
    constexpr UINT16    VendorId            = 0x955;
    constexpr UINT08    StructuredVDM       = 0x1;
    constexpr UINT08    ObjectPositionIdx   = 0x1;
    constexpr FieldDesc Svid                = { 16, 16 }; // Bits 16..31
    constexpr FieldDesc VdmType             = { 15, 1 };  // Bit 15
    constexpr FieldDesc ObjectPosition      = { 8, 3 };   // Bits 8..10
    constexpr FieldDesc HeaderCommand       = { 0, 5 };   // Bits 0..4
    constexpr FieldDesc Delay               = { 12, 11 }; // Bits 12..22
    constexpr FieldDesc EventType           = { 0, 1 };   // Bit 0
    constexpr FieldDesc CableFlipCount      = { 0, 4 };   // Bits 0..3
    constexpr FieldDesc LimitModes          = { 0, 8 };   // Bits 0..7

    // Different commands supported
    enum HeaderCommand : UINT08
    {
        USB_CMD_CABLE_ATTACH_DETACH        = 0x10,
        USB_CMD_CABLE_FLIP                 = 0x11,
        USB_CMD_LIMIT_SUPPORTED_ALT_MODE   = 0x12,
        USB_CMD_LIMIT_SUPPORTED_POWER_MODE = 0x13,
        USB_CMD_IN_BAND_WAKEUP             = 0x14,
        USB_CMD_DP_Loopback                = 0x15,
        USB_CMD_RESET_FTB                  = 0x16,
        USB_CMD_FTB_VERSION                = 0x17,
        USB_CMD_ISOC_DEVICE                = 0x18,
        /* TODO: anahar - Add functionality for   *
         * USB_CMD_GET_ORIENTATION_STATUS = 0x19, */
        USB_CMD_CHANGE_ORIENTATION         = 0x1A
    };
}

//-----------------------------------------------------------------------------
VdmFtb::VdmFtb(UINT32 domain, UINT32 bus, UINT32 device, UINT32 function) :
    m_Domain(domain), m_Bus(bus), m_Device(device), m_Function(function)
{}

//-----------------------------------------------------------------------------
VdmFtb::~VdmFtb() {}

//-----------------------------------------------------------------------------
/* virtual */ RC VdmFtb::DoInitialize()
{
    return OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC VdmFtb::DoSetAltMode(PortPolicyCtrl::UsbAltModes altMode)
{
    RC rc;

    // Vendor Defined Messages for functional test board
    // 32 bits header and 32 bits body (arguments)
    vector<tuple<UINT32, UINT32>> vdms;

    UINT32 msgHeader = 0;
    UINT32 msgBody = 0;
    CHECK_RC(ConfigureAltMode(altMode, &msgHeader, &msgBody));
    vdms.push_back(make_tuple(msgHeader, msgBody));

    CHECK_RC(SendMessage(&vdms, PortPolicyCtrl::USB_CABLE_DETACH_NEEDED));

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC VdmFtb::DoSetPowerMode(PortPolicyCtrl::UsbPowerModes powerMode)
{
    RC rc;

    // Vendor Defined Messages for functional test board
    // 32 bits header and 32 bits body (arguments)
    vector<tuple<UINT32, UINT32>> vdms;

    UINT32 msgHeader = 0;
    UINT32 msgBody = 0;
    CHECK_RC(ConfigurePowerMode(powerMode, &msgHeader, &msgBody));
    vdms.push_back(make_tuple(msgHeader, msgBody));

    CHECK_RC(SendMessage(&vdms, PortPolicyCtrl::USB_CABLE_DETACH_NOT_NEEDED));

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC VdmFtb::DoSetOrientation
(
    PortPolicyCtrl::CableOrientation orient,
    UINT16 orientDelay
)
{
    RC rc;

    // Vendor Defined Messages for functional test board
    // 32 bits header and 32 bits body (arguments)
    vector<tuple<UINT32, UINT32>> vdms;

    UINT32 msgHeader = 0;
    UINT32 msgBody = 0;
    CHECK_RC(ConfigureOrientation(orientDelay, orient, &msgHeader, &msgBody));
    vdms.push_back(make_tuple(msgHeader, msgBody));

    CHECK_RC(SendMessage(&vdms, PortPolicyCtrl::USB_CABLE_DETACH_NEEDED));

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC VdmFtb::DoReverseOrientation()
{
    RC rc;

    // Vendor Defined Messages for functional test board
    // 32 bits header and 32 bits body (arguments)
    vector<tuple<UINT32, UINT32>> vdms;

    UINT32 msgHeader = 0;
    UINT32 msgBody = 0;
    CHECK_RC(ConfigureReverseOrientation(&msgHeader, &msgBody));
    vdms.push_back(make_tuple(msgHeader, msgBody));

    CHECK_RC(SendMessage(&vdms, PortPolicyCtrl::USB_CABLE_DETACH_NEEDED));

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC VdmFtb::DoGenerateCableAttachDetach
(
    PortPolicyCtrl::CableAttachDetach eventType,
    UINT16 detachAttachDelay
)
{
    RC rc;

    // Vendor Defined Messages for functional test board
    // 32 bits header and 32 bits body (arguments)
    vector<tuple<UINT32, UINT32>> vdms;

    UINT32 msgHeader = 0;
    UINT32 msgBody = 0;
    CHECK_RC(ConfigureAttachDetach(detachAttachDelay, eventType, &msgHeader, &msgBody));
    vdms.push_back(make_tuple(msgHeader, msgBody));

    CHECK_RC(SendMessage(&vdms, PortPolicyCtrl::USB_CABLE_DETACH_NOT_NEEDED));

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC VdmFtb::DoReset()
{
    RC rc;

    // Vendor Defined Messages for functional test board
    // 32 bits header and 32 bits body (arguments)
    vector<tuple<UINT32, UINT32>> vdms;

    UINT32 msgHeader = 0;
    UINT32 msgBody = 0;
    CHECK_RC(ConfigureResetFtb(&msgHeader, &msgBody));
    vdms.push_back(make_tuple(msgHeader, msgBody));

    CHECK_RC(SendMessage(&vdms, PortPolicyCtrl::USB_CABLE_DETACH_NOT_NEEDED));

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC VdmFtb::DoGenerateInBandWakeup(UINT16 inBandDelay)
{
    RC rc;

    // Vendor Defined Messages for functional test board
    // 32 bits header and 32 bits body (arguments)
    vector<tuple<UINT32, UINT32>> vdms;

    UINT32 msgHeader = 0;
    UINT32 msgBody = 0;
    CHECK_RC(ConfigureInBandWakeup(inBandDelay, &msgHeader, &msgBody));
    vdms.push_back(make_tuple(msgHeader, msgBody));

    CHECK_RC(SendMessage(&vdms, PortPolicyCtrl::USB_CABLE_DETACH_NOT_NEEDED));

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC VdmFtb::DoSetDpLoopback(PortPolicyCtrl::DpLoopback dpLoopback)
{
    RC rc;

    // Vendor Defined Messages for functional test board
    // 32 bits header and 32 bits body (arguments)
    vector<tuple<UINT32, UINT32>> vdms;

    UINT32 msgHeader = 0;
    UINT32 msgBody = 0;
    CHECK_RC(ConfigureDpLoopback(dpLoopback, &msgHeader, &msgBody));
    vdms.push_back(make_tuple(msgHeader, msgBody));

    CHECK_RC(SendMessage(&vdms, PortPolicyCtrl::USB_CABLE_DETACH_NEEDED));

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC VdmFtb::DoSetIsocDevice(PortPolicyCtrl::IsocDevice enableIsocDev)
{
    RC rc;

    // Vendor Defined Messages for functional test board
    // 32 bits header and 32 bits body (arguments)
    vector<tuple<UINT32, UINT32>> vdms;

    UINT32 msgHeader = 0;
    UINT32 msgBody = 0;

    CHECK_RC(ConfigureIsocDevice(enableIsocDev, &msgHeader, &msgBody));
    vdms.push_back(make_tuple(msgHeader, msgBody));

    CHECK_RC(SendMessage(&vdms, PortPolicyCtrl::USB_CABLE_DETACH_NEEDED));

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC VdmFtb::DoGetLwrrUsbAltMode(PortPolicyCtrl::UsbAltModes* pAltMode)
{
    RC rc;

    CHECK_RC(Xp::GetLwrrUsbAltMode(m_Domain,
                                   m_Bus,
                                   m_Device,
                                   m_Function,
                                   reinterpret_cast<UINT08 *>(pAltMode)));

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC VdmFtb::DoConfirmUsbAltModeConfig(PortPolicyCtrl::UsbAltModes altMode)
{
    RC rc;

    CHECK_RC(Xp::ConfirmUsbAltModeConfig(m_Domain, m_Bus, m_Device, m_Function, altMode));

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC VdmFtb::DoGetPpcFwVersion
(
    string *pPrimaryVersion,
    string *pSecondaryVersion
)
{
    MASSERT(pPrimaryVersion);
    MASSERT(pSecondaryVersion);

    return Xp::GetPpcFwVersion(m_Domain,
                               m_Bus,
                               m_Device,
                               m_Function,
                               pPrimaryVersion,
                               pSecondaryVersion);
}

//-----------------------------------------------------------------------------
/* virtual */ RC VdmFtb::DoGetPpcDriverVersion(string *pDriverVersion)
{
    MASSERT(pDriverVersion);

    return Xp::GetPpcDriverVersion(m_Domain, m_Bus, m_Device, m_Function, pDriverVersion);
}

//-----------------------------------------------------------------------------
/* virtual */ RC VdmFtb::DoGetFtbFwVersion(string *pFtbFwVersion)
{
    RC rc;
    MASSERT(pFtbFwVersion);

    // Vendor Defined Messages for functional test board
    // 32 bits header and 32 bits body (arguments)
    vector<tuple<UINT32, UINT32>> vdms;

    UINT32 msgHeader = 0;
    UINT32 msgBody = 0;

    CHECK_RC(ConfigureGetFtbFwVersion(&msgHeader, &msgBody));
    vdms.push_back(make_tuple(msgHeader, msgBody));

    CHECK_RC(SendMessage(&vdms, PortPolicyCtrl::USB_CABLE_DETACH_NOT_NEEDED));

    return Xp::GetFtbFwVersion(m_Domain, m_Bus, m_Device, m_Function, pFtbFwVersion);
}

//-----------------------------------------------------------------------------
/* virtual */ RC VdmFtb::DoGetPowerDrawn(FLOAT64 *pVoltageV, FLOAT64 *pLwrrentA)
{
    RC rc;

    MASSERT(pVoltageV);
    MASSERT(pLwrrentA);

    return Xp::GetUsbPowerConsumption(m_Domain, m_Bus, m_Device, m_Function, pVoltageV, pLwrrentA);
}

//-----------------------------------------------------------------------------
RC VdmFtb::ConfigureAltMode
(
    PortPolicyCtrl::UsbAltModes altMode,
    UINT32 *pMsgHeader,
    UINT32 *pMsgBody
)
{
    RC rc;

    MASSERT(pMsgHeader);
    MASSERT(pMsgBody);

    Bitfield<UINT32, VdmHeaderBodySize> headerMsg;
    headerMsg.SetBits(Svid.offset, Svid.size, VendorId);
    headerMsg.SetBits(VdmType.offset, VdmType.size, StructuredVDM);
    headerMsg.SetBits(ObjectPosition.offset, ObjectPosition.size, ObjectPositionIdx);
    headerMsg.SetBits(HeaderCommand.offset, HeaderCommand.size, USB_CMD_LIMIT_SUPPORTED_ALT_MODE);
    *pMsgHeader =  *(headerMsg.GetData());

    Bitfield<UINT32, VdmHeaderBodySize> bodyMsg;
    // Each bit (0..7) decides whether parilwlar ALT mode is supported (bit=1) or no (bit=0)
    // With FTB FW ver 2.5, it is expected that USB_LW_TEST_ALT_MODE is always supported.
    // So with each ALT mode switch we need to request for LW_TEST_ALT_MODE as well.
    // This bit will be ignored by FTB FW ver 2.4 and below
    bodyMsg.SetBits(LimitModes.offset,
                    LimitModes.size,
                    (1 << altMode) | (1 << PortPolicyCtrl::USB_LW_TEST_ALT_MODE));
    *pMsgBody = *(bodyMsg.GetData());

    return rc;
}

//-----------------------------------------------------------------------------
RC VdmFtb::ConfigurePowerMode
(
    PortPolicyCtrl::UsbPowerModes powerMode,
    UINT32 *pMsgHeader,
    UINT32 *pMsgBody
)
{
    RC rc;

    MASSERT(pMsgHeader);
    MASSERT(pMsgBody);

    Bitfield<UINT32, VdmHeaderBodySize> headerMsg;
    headerMsg.SetBits(Svid.offset, Svid.size, VendorId);
    headerMsg.SetBits(VdmType.offset, VdmType.size, StructuredVDM);
    headerMsg.SetBits(ObjectPosition.offset, ObjectPosition.size, ObjectPositionIdx);
    headerMsg.SetBits(HeaderCommand.offset, HeaderCommand.size,
                      USB_CMD_LIMIT_SUPPORTED_POWER_MODE);
    *pMsgHeader = *(headerMsg.GetData());

    Bitfield<UINT32, VdmHeaderBodySize> bodyMsg;
    // Each bit (0..7) decides whether parilwlar power mode is supported (bit=1) or no (bit=0)
    // Value of 0 means that we are resetting the power mode to default
    if (powerMode != PortPolicyCtrl::USB_DEFAULT_POWER_MODE)
    {
        bodyMsg.SetBits(LimitModes.offset, LimitModes.size, 1 << powerMode);
    }
    *pMsgBody = *(bodyMsg.GetData());

    return rc;
}

//-----------------------------------------------------------------------------
RC VdmFtb::ConfigureOrientation
(
    UINT16 delay,
    PortPolicyCtrl::CableOrientation orient,
    UINT32 *pMsgHeader,
    UINT32 *pMsgBody
)
{
    RC rc;

    MASSERT(pMsgHeader);
    MASSERT(pMsgBody);

    Bitfield<UINT32, VdmHeaderBodySize> headerMsg;
    headerMsg.SetBits(Svid.offset, Svid.size, VendorId);
    headerMsg.SetBits(VdmType.offset, VdmType.size, StructuredVDM);
    headerMsg.SetBits(ObjectPosition.offset, ObjectPosition.size, ObjectPositionIdx);
    headerMsg.SetBits(HeaderCommand.offset, HeaderCommand.size, USB_CMD_CABLE_FLIP);
    *pMsgHeader = *(headerMsg.GetData());

    Bitfield<UINT32, VdmHeaderBodySize> bodyMsg;
    bodyMsg.SetBits(Delay.offset, Delay.size, delay);
    bodyMsg.SetBits(CableFlipCount.offset, CableFlipCount.size, orient);
    *pMsgBody = *(bodyMsg.GetData());

    return rc;
}

//-----------------------------------------------------------------------------
RC VdmFtb::ConfigureReverseOrientation
(
    UINT32 *pMsgHeader,
    UINT32 *pMsgBody
)
{
    RC rc;

    MASSERT(pMsgHeader);
    MASSERT(pMsgBody);

    Bitfield<UINT32, VdmHeaderBodySize> headerMsg;
    headerMsg.SetBits(Svid.offset, Svid.size, VendorId);
    headerMsg.SetBits(VdmType.offset, VdmType.size, StructuredVDM);
    headerMsg.SetBits(ObjectPosition.offset, ObjectPosition.size, ObjectPositionIdx);
    headerMsg.SetBits(HeaderCommand.offset, HeaderCommand.size, USB_CMD_CHANGE_ORIENTATION);
    *pMsgHeader = *(headerMsg.GetData());

    *pMsgBody = 0;

    return rc;
}

//-----------------------------------------------------------------------------
RC VdmFtb::ConfigureAttachDetach
(
    UINT16 delay,
    PortPolicyCtrl::CableAttachDetach eventType,
    UINT32 *pMsgHeader,
    UINT32 *pMsgBody
)
{
    RC rc;

    MASSERT(pMsgHeader);
    MASSERT(pMsgBody);

    Bitfield<UINT32, VdmHeaderBodySize> headerMsg;
    headerMsg.SetBits(Svid.offset, Svid.size, VendorId);
    headerMsg.SetBits(VdmType.offset, VdmType.size, StructuredVDM);
    headerMsg.SetBits(ObjectPosition.offset, ObjectPosition.size, ObjectPositionIdx);
    headerMsg.SetBits(HeaderCommand.offset, HeaderCommand.size, USB_CMD_CABLE_ATTACH_DETACH);
    *pMsgHeader = *(headerMsg.GetData());

    Bitfield<UINT32, VdmHeaderBodySize> bodyMsg;
    bodyMsg.SetBits(Delay.offset, Delay.size, delay);
    bodyMsg.SetBits(EventType.offset, EventType.size, eventType);
    *pMsgBody = *(bodyMsg.GetData());

    return rc;
}

//-----------------------------------------------------------------------------
RC VdmFtb::ConfigureInBandWakeup(UINT16 delay, UINT32 *pMsgHeader, UINT32 *pMsgBody)
{
    RC rc;

    MASSERT(pMsgHeader);
    MASSERT(pMsgBody);

    Bitfield<UINT32, VdmHeaderBodySize> headerMsg;
    headerMsg.SetBits(Svid.offset, Svid.size, VendorId);
    headerMsg.SetBits(VdmType.offset, VdmType.size, StructuredVDM);
    headerMsg.SetBits(ObjectPosition.offset, ObjectPosition.size, ObjectPositionIdx);
    headerMsg.SetBits(HeaderCommand.offset, HeaderCommand.size, USB_CMD_IN_BAND_WAKEUP);
    *pMsgHeader = *(headerMsg.GetData());

    Bitfield<UINT32, VdmHeaderBodySize> bodyMsg;
    bodyMsg.SetBits(Delay.offset, Delay.size, delay);
    *pMsgBody = *(bodyMsg.GetData());

    return rc;
}

//-----------------------------------------------------------------------------
RC VdmFtb::ConfigureDpLoopback
(
    PortPolicyCtrl::DpLoopback dpLoopback,
    UINT32 *pMsgHeader,
    UINT32 *pMsgBody
)
{
    RC rc;

    MASSERT(pMsgHeader);
    MASSERT(pMsgBody);

    Bitfield<UINT32, VdmHeaderBodySize> headerMsg;
    headerMsg.SetBits(Svid.offset, Svid.size, VendorId);
    headerMsg.SetBits(VdmType.offset, VdmType.size, StructuredVDM);
    headerMsg.SetBits(ObjectPosition.offset, ObjectPosition.size, ObjectPositionIdx);
    headerMsg.SetBits(HeaderCommand.offset, HeaderCommand.size, USB_CMD_DP_Loopback);
    *pMsgHeader = *(headerMsg.GetData());

    Bitfield<UINT32, VdmHeaderBodySize> bodyMsg;
    bodyMsg.SetBits(EventType.offset, EventType.size, dpLoopback);
    *pMsgBody = *(bodyMsg.GetData());

    return rc;
}

//-----------------------------------------------------------------------------
RC VdmFtb::ConfigureIsocDevice
(
    PortPolicyCtrl::IsocDevice isocDevice,
    UINT32 *pMsgHeader,
    UINT32 *pMsgBody
)
{
    RC rc;

    MASSERT(pMsgHeader);
    MASSERT(pMsgBody);

    Bitfield<UINT32, VdmHeaderBodySize> headerMsg;
    headerMsg.SetBits(Svid.offset, Svid.size, VendorId);
    headerMsg.SetBits(VdmType.offset, VdmType.size, StructuredVDM);
    headerMsg.SetBits(ObjectPosition.offset, ObjectPosition.size, ObjectPositionIdx);
    headerMsg.SetBits(HeaderCommand.offset, HeaderCommand.size, USB_CMD_ISOC_DEVICE);
    *pMsgHeader = *(headerMsg.GetData());

    Bitfield<UINT32, VdmHeaderBodySize> bodyMsg;
    bodyMsg.SetBits(EventType.offset, EventType.size, isocDevice);
    *pMsgBody = *(bodyMsg.GetData());

    return rc;
}

//-----------------------------------------------------------------------------
RC VdmFtb::ConfigureResetFtb(UINT32 *pMsgHeader, UINT32 *pMsgBody)
{
    RC rc;

    MASSERT(pMsgHeader);
    MASSERT(pMsgBody);

    Bitfield<UINT32, VdmHeaderBodySize> headerMsg;
    headerMsg.SetBits(Svid.offset, Svid.size, VendorId);
    headerMsg.SetBits(VdmType.offset, VdmType.size, StructuredVDM);
    headerMsg.SetBits(ObjectPosition.offset, ObjectPosition.size, ObjectPositionIdx);
    headerMsg.SetBits(HeaderCommand.offset, HeaderCommand.size, USB_CMD_RESET_FTB);
    *pMsgHeader = *(headerMsg.GetData());

    *pMsgBody = 0x0;

    return rc;
}

//-----------------------------------------------------------------------------
RC VdmFtb::ConfigureGetFtbFwVersion(UINT32 *pMsgHeader, UINT32 *pMsgBody)
{
    RC rc;

    MASSERT(pMsgHeader);
    MASSERT(pMsgBody);

    Bitfield<UINT32, VdmHeaderBodySize> headerMsg;
    headerMsg.SetBits(Svid.offset, Svid.size, VendorId);
    headerMsg.SetBits(VdmType.offset, VdmType.size, StructuredVDM);
    headerMsg.SetBits(ObjectPosition.offset, ObjectPosition.size, ObjectPositionIdx);
    headerMsg.SetBits(HeaderCommand.offset, HeaderCommand.size, USB_CMD_FTB_VERSION);
    *pMsgHeader = *(headerMsg.GetData());

    *pMsgBody = 0x0;

    return rc;
}

//-----------------------------------------------------------------------------
RC VdmFtb::SendMessage
(
    vector<tuple<UINT32, UINT32>> *pVdms,
    PortPolicyCtrl::AddCableDetach addDetachCableVdm
)
{
    RC rc;
    MASSERT(pVdms);

    const UINT32 detachAttachDelay = 0;

    if (addDetachCableVdm == PortPolicyCtrl::USB_CABLE_DETACH_NEEDED)
    {
        UINT32 msgHeader = 0;
        UINT32 msgBody = 0;
        CHECK_RC(ConfigureAttachDetach(detachAttachDelay, PortPolicyCtrl::USB_CABLE_DETACH,
                                       &msgHeader, &msgBody));
        pVdms->push_back(make_tuple(msgHeader, msgBody));
    }

    CHECK_RC_MSG(Xp::SendMessageToFTB(m_Domain, m_Bus, m_Device, m_Function, *pVdms),
                 "Functional test board configuration failed!");

    return rc;
}

