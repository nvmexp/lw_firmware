/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwmisc.h"
#include "core/include/pci.h"
#include "core/include/platform.h"
#include "ctrl/ctrl0000.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/testdevice.h"
#include "gpu/usb/hostctrl/xusbhostctrlimpl.h"
#include "gpu/usb/usbdatatransfer.h"
#include "lw_ref.h"

namespace
{
    const UINT32 XUSB_HOST_CTRL_BAR0_INDEX      = 0x0;
    const UINT32 XUSB_HOST_CTRL_MAX_PORTS       = 16;
}

//-----------------------------------------------------------------------------
/* virtual */ XusbHostCtrlImpl::~XusbHostCtrlImpl()
{
    DoUnMapAvailBARs();
}

//-----------------------------------------------------------------------------
/* virtual */ bool XusbHostCtrlImpl::DoIsSupported() const
{
    if (const_cast<XusbHostCtrlImpl*>(this)->SupportInitHelper() != OK)
    {
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
/* virtual */ RC XusbHostCtrlImpl::DoGetPciDBDF
(
    UINT32 *pDomain,
    UINT32 *pBus,
    UINT32 *pDevice,
    UINT32 *pFunction
) const
{
    if (!m_Init)
    {
        Printf(Tee::PriError, "XUSB Host Controller not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    if (pDomain)
    {
        *pDomain = m_Domain;
    }
    if (pBus)
    {
        *pBus = m_Bus;
    }
    if (pDevice)
    {
        *pDevice = m_Device;
    }
    if (pFunction)
    {
        *pFunction = m_Function;
    }

    return OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC XusbHostCtrlImpl::DoInitialize()
{
    RC rc;
    rc = SupportInitHelper();
    if (rc != OK)
    {
        Printf(Tee::PriError,
               "Initialization of XUSB host controller failed\n");
        return rc;
    }

    Printf(Tee::PriLow,
           "XUSB Host Controller at %04x:%x:%02x.%x initialized\n",
           m_Domain,
           m_Bus,
           m_Device,
           m_Function);

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC XusbHostCtrlImpl::DoMapAvailBARs()
{
    RC rc;

    if (!m_Init)
    {
        Printf(Tee::PriError, "XUSB Host Controller not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    if (m_pBar0Addr)
    {
        Printf(Tee::PriLow, "BAR information for XUSB Host Controller already mapped\n");
        return OK;
    }

    CHECK_RC(MapBAR(XUSB_HOST_CTRL_BAR0_INDEX, &m_pBar0Addr));

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC XusbHostCtrlImpl::DoUnMapAvailBARs()
{
    if (m_pBar0Addr)
    {
        Platform::UnMapDeviceMemory(m_pBar0Addr);
        m_pBar0Addr = nullptr;
    }
    return OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC XusbHostCtrlImpl::DoIsLowPowerStateEnabled(bool *pIsHcInLowPowerMode)
{
    RC rc;

    MASSERT(pIsHcInLowPowerMode);

    if (!m_Init)
    {
        Printf(Tee::PriError, "XUSB Host Controller not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    // Find the Capability header for Power Management
    UINT08 capPtr = 0;
    CHECK_RC(Pci::FindCapBase(m_Domain,
                              m_Bus,
                              m_Device,
                              m_Function,
                              PCI_CAP_ID_POWER_MGT,
                              &capPtr));

    // Extract Power Management Control and Status register from 
    // Power Management Capability header
    UINT16 pwrMgtCtrlSts = 0;
    CHECK_RC(Platform::PciRead16(m_Domain,
                                 m_Bus,
                                 m_Device,
                                 m_Function,
                                 capPtr + LW_PCI_CAP_POWER_MGT_CTRL_STS,
                                 &pwrMgtCtrlSts));

    *pIsHcInLowPowerMode = false;
    string powerState = "D0";
    // Check if Power state is D3
    if (FLD_TEST_DRF(_PCI_CAP, _POWER_MGT_CTRL_STS, _POWER_STATE, _PS_D3, pwrMgtCtrlSts))
    {
        *pIsHcInLowPowerMode = true;
        powerState = "D3";
    }
    Printf(Tee::PriLow,
           "XUSB Host Controller at %04x:%x:%02x.%x is in %s power state\n",
           m_Domain,
           m_Bus,
           m_Device,
           m_Function,
           powerState.c_str());

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC XusbHostCtrlImpl::DoEnableLowPowerState(UINT32 autoSuspendDelayMs)
{
    RC rc;

    if (!m_Init)
    {
        Printf(Tee::PriError, "XUSB Host Controller not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    bool isHCInLowPowerMode = false;

    // Check if XUSB host controller is in D3 state
    CHECK_RC(IsLowPowerStateEnabled(&isHCInLowPowerMode));
    if (isHCInLowPowerMode)
    {
        return rc;
    }

    bool unMapBar = false;
    if (!m_pBar0Addr)
    {
        CHECK_RC(MapAvailBARs());
        unMapBar = true;
    }
    DEFER
    {
        if (unMapBar)
        {
            UnMapAvailBARs();
        }
    };

    // Unable Low Power for external devices and also for XUSB host controller
    CHECK_RC(Xp::EnableUsbAutoSuspend(m_Domain,
                                      m_Bus,
                                      m_Device,
                                      m_Function,
                                      autoSuspendDelayMs));

    // Check if XUSB host controller is in D3 state
    CHECK_RC(IsLowPowerStateEnabled(&isHCInLowPowerMode));
    if (!isHCInLowPowerMode)
    {
        Printf(Tee::PriError,
               "Unable to put XUSB Host Controller at %04x:%x:%02x.%x in low power mode\n",
               m_Domain,
               m_Bus,
               m_Device,
               m_Function);
        return RC::USBTEST_CONFIG_FAIL;
    }

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC XusbHostCtrlImpl::DoUsbDeviceSpeedEnumToString
(
    UsbDeviceSpeed speed,
    string *pSpeedShortStr,
    string *pSpeedLongStr
) const
{
    RC rc;

    string speedShortStr;
    string speedLongStr;
    switch (speed)
    {
        case USB_HIGH_SPEED:
            speedShortStr = "HS";
            speedLongStr = "High Speed";
            break;

        case USB_SUPER_SPEED:
            speedShortStr = "SS";
            speedLongStr = "Super Speed";
            break;
        
        case USB_SUPER_SPEED_PLUS:
            speedShortStr = "SSP";
            speedLongStr = "Super Speed Plus";
            break;

        default:
            Printf(Tee::PriError,
                   "Unknown USB Speed (%u) selected\n",
                   static_cast<UINT08>(speed));
            return RC::USBTEST_CONFIG_FAIL;
    }

    if (pSpeedShortStr)
    {
        *pSpeedShortStr = speedShortStr;
    }
    if (pSpeedLongStr)
    {
        *pSpeedLongStr = speedLongStr;
    }

    return rc;
}


//-----------------------------------------------------------------------------
/* virtual */ RC XusbHostCtrlImpl::DoGetNumOfConnectedUsbDev
(
    UINT08 *pUsb3_1,
    UINT08 *pUsb3_0,
    UINT08 *pUsb2
)
{
    RC rc;

    if (!m_Init)
    {
        Printf(Tee::PriError, "XUSB Host Controller not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    bool unMapBar = false;
    if (!m_pBar0Addr)
    {
        CHECK_RC(MapAvailBARs());
        unMapBar = true;
    }
    DEFER
    {
        if (unMapBar)
        {
            UnMapAvailBARs();
        }
    };

    MASSERT(pUsb3_1);
    MASSERT(pUsb3_0);
    MASSERT(pUsb2);

    *pUsb3_1 = 0;
    *pUsb3_0 = 0;
    *pUsb2 = 0;

    UINT32 maxPorts = 0;
    CHECK_RC(GetNumOfPorts(&maxPorts));

    RegHal &regs = GetDevice()->Regs();
    for (UINT32 portIdx = 0; portIdx < maxPorts; portIdx++)
    {
        UINT32 regValue = 0;
        CHECK_RC(ReadReg32(m_pBar0Addr, MODS_XUSB_XHCI_OP_PORTSC, portIdx, &regValue));

        if (regs.TestField(regValue, MODS_XUSB_XHCI_OP_PORTSC_CCS_DEV))
        {
            const char* usbSpeed;
            if (regs.TestField(regValue, MODS_XUSB_XHCI_OP_PORTSC_PSPD_HS))
            {
                (*pUsb2)++;
                usbSpeed = "High Speed";
            }
            else if (regs.TestField(regValue, MODS_XUSB_XHCI_OP_PORTSC_PSPD_SS))
            {
                (*pUsb3_0)++;
                usbSpeed = "Super Speed";
            }
            else if (regs.TestField(regValue, MODS_XUSB_XHCI_OP_PORTSC_PSPD_SSP))
            {
                (*pUsb3_1)++;
                usbSpeed = "Super Speed Plus";
            }
            else
            {
                Printf(Tee::PriError,
                       "Only High speed, super speed or super speed plus device expected\n");
                return RC::USBTEST_CONFIG_FAIL;
            }
            Printf(Tee::PriLow,
                   "%s USB device found on port %u\n",
                   usbSpeed,
                   portIdx);
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC XusbHostCtrlImpl::DoGetAspmState(UINT32* pState)
{
    MASSERT(pState);
    RC rc;

    UINT08 pciCapBase = 0;
    CHECK_RC(Pci::FindCapBase(m_Domain,
                              m_Bus,
                              m_Device,
                              m_Function,
                              PCI_CAP_ID_PCIE,
                              &pciCapBase));

    UINT32 linkCtrl = 0;
    CHECK_RC(Platform::PciRead32(m_Domain,
                                 m_Bus,
                                 m_Device,
                                 m_Function,
                                 pciCapBase + LW_PCI_CAP_PCIE_LINK_CTRL,
                                 &linkCtrl));

    *pState = DRF_VAL(_PCI_CAP, _PCIE_LINK_CTRL, _ASPM, linkCtrl);

    return OK;
}

//-----------------------------------------------------------------------------
RC XusbHostCtrlImpl::DoSetAspmState(UINT32 state)
{
    RC rc;

    UINT08 pciCapBase = 0;
    CHECK_RC(Pci::FindCapBase(m_Domain,
                              m_Bus,
                              m_Device,
                              m_Function,
                              PCI_CAP_ID_PCIE,
                              &pciCapBase));

    UINT32 linkCtrl = 0;
    CHECK_RC(Platform::PciRead32(m_Domain,
                                 m_Bus,
                                 m_Device,
                                 m_Function,
                                 pciCapBase + LW_PCI_CAP_PCIE_LINK_CTRL,
                                 &linkCtrl));

    linkCtrl = FLD_SET_DRF_NUM(_PCI_CAP, _PCIE_LINK_CTRL, _ASPM, state, linkCtrl);

    CHECK_RC(Platform::PciWrite32(m_Domain,
                                  m_Bus,
                                  m_Device,
                                  m_Function,
                                  pciCapBase + LW_PCI_CAP_PCIE_LINK_CTRL,
                                  linkCtrl));

    return OK;
}

//-----------------------------------------------------------------------------
RC XusbHostCtrlImpl::FindHostController(UINT32 domain, UINT32 bus, UINT32 device, UINT32 function)
{
    RC rc;

    // Find all the device with class code = CLASS_CODE_USB30_XHCI_HOST
    // and try to map against current GPU's domain, bus and device number
    for (UINT32 index = 0; /* until all devices found or error */; index++)
    {
        UINT32 HCDomain, HCBus, HCDevice, HCFunction;
        CHECK_RC(Platform::FindPciClassCode(Pci::CLASS_CODE_USB30_XHCI_HOST,
                                            index,
                                            &HCDomain,
                                            &HCBus,
                                            &HCDevice,
                                            &HCFunction));

        if (HCDomain == domain &&
            HCBus == bus &&
            HCDevice == device &&
            HCFunction == function)
        {
            m_Domain = HCDomain;
            m_Bus = HCBus;
            m_Device = HCDevice;
            m_Function = HCFunction;
            return OK;
        }
    }
    return RC::PCI_DEVICE_NOT_FOUND;
}

//-----------------------------------------------------------------------------
RC XusbHostCtrlImpl::SupportInitHelper()
{
    RC rc;

    if (m_Init)
    {
        return rc;
    }

    if (m_IsChecked && !m_Init)
    {
        return RC::PCI_DEVICE_NOT_FOUND;
    }
    m_IsChecked = true;

    UINT32 domain, bus, device, function;
    GpuSubdevice *pSubDevice = dynamic_cast<GpuSubdevice *>(GetDevice());
    MASSERT(pSubDevice);
    UsbDataTransfer usbDataTransfer;

    rc = usbDataTransfer.Initialize(pSubDevice);
    if (rc != OK)
    {
        Printf(Tee::PriLow,
               "Initialization of Usb Data Transfer failed\n");
        return rc;
    }

    rc = usbDataTransfer.GetHostCtrlPciDBDF(&domain, &bus, &device, &function);
    if (rc != OK)
    {
        Printf(Tee::PriLow,
               "No external USB device is attached. "
               "Unable to determine the PciDBDF for host controller\n");
        return rc;
    }

    rc = FindHostController(domain, bus, device, function);
    if (rc != OK)
    {
        Printf(Tee::PriLow,
               "No Host controller found at %04x:%x:%02x.%x\n",
               domain,
               bus,
               device,
               function);
        return rc;
    }

    m_Init = true;

    return rc;
}

//-----------------------------------------------------------------------------
RC XusbHostCtrlImpl::MapBAR(UINT32 barIndex, void **ppBarAddr)
{
    RC rc;

    MASSERT(ppBarAddr);

    if (!IsBarSupported(barIndex))
    {
        Printf(Tee::PriError,
               "BAR%u not available for host controller at %04x:%x:%02x.%x\n",
               barIndex,
               m_Domain,
               m_Bus,
               m_Device,
               m_Function);
        return RC::SOFTWARE_ERROR;
    }

    // Get the BAR configuration offsets
    PHYSADDR baseAddress = 0;
    UINT64 barSize = 0;
    CHECK_RC(Platform::PciGetBarInfo(m_Domain,
                                     m_Bus,
                                     m_Device,
                                     m_Function,
                                     barIndex,
                                     &baseAddress,
                                     &barSize));

    // Make sure it is a valid BAR0 address
    if ((0 == baseAddress) ||
        (~0xFU == baseAddress) ||
        (~UINT64(0xFU) == baseAddress) ||
        (0 == barSize))
    {
        Printf(Tee::PriError,
               "Incorrect BAR%u - offset 0x%llx, size 0x%llx\n",
               barIndex,
               baseAddress,
               barSize);
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    // Enable the memory space and bus mastering, map the memory.
    UINT32 pciLw1 = 0;
    CHECK_RC(Platform::PciRead32(m_Domain,
                                 m_Bus,
                                 m_Device,
                                 m_Function,
                                 LW_CONFIG_PCI_LW_1,
                                 &pciLw1));

    pciLw1 |= DRF_DEF(_CONFIG, _PCI_LW_1, _MEMORY_SPACE, _ENABLED)
              | DRF_DEF(_CONFIG, _PCI_LW_1, _BUS_MASTER, _ENABLED);

    CHECK_RC(Platform::PciWrite32(m_Domain,
                                  m_Bus,
                                  m_Device,
                                  m_Function,
                                  LW_CONFIG_PCI_LW_1,
                                  pciLw1));

    // Map GPU registers
    // Memory will be unmapped in the destructor
    void* pBar = nullptr;
    CHECK_RC(Platform::MapDeviceMemory(&pBar,
                                       baseAddress,
                                       static_cast<UINT32>(barSize),
                                       Memory::UC,
                                       Memory::ReadWrite));
    *ppBarAddr = pBar;

    return rc;
}

//-----------------------------------------------------------------------------
bool XusbHostCtrlImpl::IsBarSupported(UINT32 barIndex)
{
    switch (barIndex)
    {
        // Only BAR0 is supported on standard host controllers
        case XUSB_HOST_CTRL_BAR0_INDEX:
            return true;

        default:
            return false;
    }
}

//-----------------------------------------------------------------------------
RC XusbHostCtrlImpl::GetNumOfPorts(UINT32 *pNumOfPorts)
{
    RC rc;
    UINT32 maxPorts;
    MASSERT(pNumOfPorts);

    // Retrieve the available number of ports on the Host Controller
    UINT32 regVal = 0;
    CHECK_RC(ReadReg32(m_pBar0Addr, MODS_XUSB_XHCI_CAP_HCSPARAMS1, &regVal));
    maxPorts = GetDevice()->Regs().GetField(regVal, MODS_XUSB_XHCI_CAP_HCSPARAMS1_MAXPORTS);

    if (maxPorts > XUSB_HOST_CTRL_MAX_PORTS)
    {
        // On Turing we have 6 (internal) ports on the XUSB host controller,
        // register manuals supports 16 registers
        // On a standard host controller we can have more than 16 ports and
        // register manuals for Volta (XUSB related) are copied from Turing
        Printf(Tee::PriError,
               "XUSB Host Controller has %u ports, only %u ports supported\n",
               maxPorts,
               XUSB_HOST_CTRL_MAX_PORTS);
        return RC::USBTEST_CONFIG_FAIL;
    }
    *pNumOfPorts = maxPorts;

    return rc;
}

//-----------------------------------------------------------------------------
RC XusbHostCtrlImpl::GetPortNumSpeedOfConnectedDev
(
    LaneDeviceInfo *pLaneDeviceUsb3Info,
    LaneDeviceInfo *pLaneDeviceUsb2Info
)
{
    RC rc;

    if (!m_Init)
    {
        Printf(Tee::PriError, "XUSB Host Controller not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    if (!m_pBar0Addr)
    {
        Printf(Tee::PriError, "BAR0 information for XUSB Host Controller not mapped\n");
        return RC::SOFTWARE_ERROR;
    }

    UINT32 maxPorts = 0;
    CHECK_RC(GetNumOfPorts(&maxPorts));

    INT32 usb3PortNum = -1;
    INT32 usb2PortNum = -1;
    UsbDeviceSpeed usb2DeviceSpeed = USB_UNKNOWN_DATA_RATE;
    UsbDeviceSpeed usb3DeviceSpeed = USB_UNKNOWN_DATA_RATE;

    RegHal &regs = GetDevice()->Regs();
    for (UINT32 portIdx = 0; portIdx < maxPorts; portIdx++)
    {
        UINT32 regValue = 0;
        CHECK_RC(ReadReg32(m_pBar0Addr, MODS_XUSB_XHCI_OP_PORTSC, portIdx, &regValue));

        if (regs.TestField(regValue, MODS_XUSB_XHCI_OP_PORTSC_CCS_DEV))
        {
            if (regs.TestField(regValue, MODS_XUSB_XHCI_OP_PORTSC_PSPD_HS))
            {
                if (usb2PortNum != -1)
                {
                    Printf(Tee::PriError,
                           "USB 2 device found on multiple ports (%d, %u)\n",
                           usb2PortNum,
                           portIdx);
                    return RC::USBTEST_CONFIG_FAIL;
                }
                usb2PortNum = static_cast<INT32>(portIdx);
                usb2DeviceSpeed = USB_HIGH_SPEED;
            }
            else if (regs.TestField(regValue, MODS_XUSB_XHCI_OP_PORTSC_PSPD_SS))
            {
                if (usb3PortNum != -1)
                {
                    Printf(Tee::PriError,
                           "USB 3 device found on multiple ports (%d, %u)\n",
                           usb3PortNum,
                           portIdx);
                    return RC::USBTEST_CONFIG_FAIL;
                }
                usb3PortNum = static_cast<INT32>(portIdx);
                usb3DeviceSpeed = USB_SUPER_SPEED;
            }
            else if (regs.TestField(regValue, MODS_XUSB_XHCI_OP_PORTSC_PSPD_SSP))
            {
                if (usb3PortNum != -1)
                {
                    Printf(Tee::PriError,
                           "USB 3 device found on multiple ports (%d, %u)\n",
                           usb3PortNum,
                           portIdx);
                    return RC::USBTEST_CONFIG_FAIL;
                }
                usb3PortNum = static_cast<INT32>(portIdx);
                usb3DeviceSpeed = USB_SUPER_SPEED_PLUS;
            }
            else
            {
                Printf(Tee::PriError,
                       "Only High speed, super speed or super speed plus device expected\n");
                return RC::USBTEST_CONFIG_FAIL;
            }
        }
    }

    if (pLaneDeviceUsb2Info)
    {
        pLaneDeviceUsb2Info->portNum = usb2PortNum;
        pLaneDeviceUsb2Info->speed = usb2DeviceSpeed;
    }

    if (pLaneDeviceUsb3Info)
    {
        pLaneDeviceUsb3Info->portNum = usb3PortNum;
        pLaneDeviceUsb3Info->speed = usb3DeviceSpeed;
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC XusbHostCtrlImpl::ReadReg32
(
    void *pBar,
    ModsGpuRegAddress regAddr,
    UINT32 *pRegVal
)
{
    RC rc;
    MASSERT(pBar);
    MASSERT(pRegVal);

    RegHal &regs = GetDevice()->Regs();
    if (!regs.IsSupported(regAddr))
    {
        Printf(Tee::PriError,
               "Register not supported %s\n",
               regs.ColwertToString(regAddr));
        return RC::REGISTER_READ_WRITE_FAILED;
    }

    const UINT32 regOffset = regs.LookupAddress(regAddr);
    CHECK_RC(ReadReg32(pBar, regOffset, pRegVal));

    return rc;
}

//-----------------------------------------------------------------------------
RC XusbHostCtrlImpl::ReadReg32
(
    void *pBar,
    ModsGpuRegAddress regAddr,
    UINT32 idx,
    UINT32 *pRegVal
)
{
    RC rc;
    MASSERT(pBar);
    MASSERT(pRegVal);

    RegHal &regs = GetDevice()->Regs();
    if (!regs.IsSupported(regAddr, idx))
    {
        Printf(Tee::PriError,
               "Register not supported %s(%d)\n",
               regs.ColwertToString(regAddr),
               idx);
        return RC::REGISTER_READ_WRITE_FAILED;
    }

    const UINT32 regOffset = regs.LookupAddress(regAddr, idx);
    CHECK_RC(ReadReg32(pBar, regOffset, pRegVal));

    return rc;
}

//-----------------------------------------------------------------------------
RC XusbHostCtrlImpl::ReadReg32
(
    void *pBar,
    UINT32 regOffset,
    UINT32 *pRegVal
)
{

    RC rc;
    MASSERT(pBar);
    MASSERT(pRegVal);

    *pRegVal = *(static_cast<UINT32 *>(pBar) + (regOffset >> 2));

    return rc;
}

//-----------------------------------------------------------------------------
RC XusbHostCtrlImpl::WriteReg32
(
    void *pBar,
    ModsGpuRegAddress regAddr,
    UINT32 regVal
)
{
    RC rc;
    MASSERT(pBar);

    RegHal &regs = GetDevice()->Regs();
    if (!regs.IsSupported(regAddr))
    {
        Printf(Tee::PriError,
               "Register not supported %s\n",
               regs.ColwertToString(regAddr));
        return RC::REGISTER_READ_WRITE_FAILED;
    }

    const UINT32 regOffset = regs.LookupAddress(regAddr);
    CHECK_RC(WriteReg32(pBar, regOffset, regVal));

    return rc;
}

//-----------------------------------------------------------------------------
RC XusbHostCtrlImpl::WriteReg32
(
    void *pBar,
    UINT32 regOffset,
    UINT32 regVal
)
{
    RC rc;
    MASSERT(pBar);

    *(static_cast<UINT32 *>(pBar) + (regOffset >> 2)) = regVal;

    return rc;
}
