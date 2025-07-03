/*
 *
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2001-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
// $Id: //sw/integ/gpu_drv/stage_rel/diag/memory/mats2/src/linuxwrap.cpp#9 $
//------------------------------------------------------------------------------
// 45678901234567890123456789012345678901234567890123456789012345678901234567890

#include "comnmats.h"
#include "mods.h"
#include <stdio.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

int s_KrnFd = -1;
const char* s_DeviceFileName = "/dev/mods";
bool s_DomainApiSupport = false;

//------------------------------------------------------------------------------
// FindVideoController
//------------------------------------------------------------------------------

bool FindVideoController
(
   UINT32   Index,
   UINT32   ClassCode,
   UINT32 * pDomainNumber,
   UINT32 * pBusNumber,
   UINT32 * pDevice,
   UINT32 * pFunction
)
{
    if (s_DomainApiSupport)
    {
        MODS_FIND_PCI_CLASS_CODE_2 ModsFindPciClassCodeData = {};

        // set private members
        ModsFindPciClassCodeData.class_code  = ClassCode;
        ModsFindPciClassCodeData.index       = Index;

        if (0 != ioctl(s_KrnFd, MODS_ESC_FIND_PCI_CLASS_CODE_2,
                    &ModsFindPciClassCodeData))
        {
            Printf(Tee::PriHigh, "Error: Can't find Pci device with class code 0x%x,"
                " index %d \n",
                    ClassCode, Index);
            return false;
        }

        *pDomainNumber      = ModsFindPciClassCodeData.pci_device.domain;
        *pBusNumber         = ModsFindPciClassCodeData.pci_device.bus;
        *pDevice            = ModsFindPciClassCodeData.pci_device.device;
        *pFunction          = ModsFindPciClassCodeData.pci_device.function;
        return true;
    }
    else
    {
        MODS_FIND_PCI_CLASS_CODE ModsFindPciClassCodeData = {};

        // set private members
        ModsFindPciClassCodeData.class_code  = ClassCode;
        ModsFindPciClassCodeData.index       = Index;

        if (0 != ioctl(s_KrnFd, MODS_ESC_FIND_PCI_CLASS_CODE,
                    &ModsFindPciClassCodeData))
        {
            Printf(Tee::PriHigh, "Error: Can't find Pci device with class code 0x%x,"
                " index %d \n",
                    ClassCode, Index);
            return false;
        }

        *pDomainNumber      = 0;
        *pBusNumber         = ModsFindPciClassCodeData.bus_number;
        *pDevice            = ModsFindPciClassCodeData.device_number;
        *pFunction          = ModsFindPciClassCodeData.function_number;
        return true;
    }
}

//------------------------------------------------------------------------------
// PciRead32
//------------------------------------------------------------------------------

UINT32 PciRead32
(
   UINT32 DomainNumber,
   UINT32 BusNumber,
   UINT32 Device,
   UINT32 Function,
   UINT32 Register
)
{
    if (s_DomainApiSupport)
    {
        MODS_PCI_READ_2   ModsPciReadData = {};

        // set private members
        ModsPciReadData.pci_device.domain    = DomainNumber;
        ModsPciReadData.pci_device.bus       = BusNumber;
        ModsPciReadData.pci_device.device    = Device;
        ModsPciReadData.pci_device.function  = Function;
        ModsPciReadData.address              = Register;
        ModsPciReadData.data_size            = 4;

        if (0 != ioctl(s_KrnFd, MODS_ESC_PCI_READ_2, &ModsPciReadData))
        {
            Printf(Tee::PriDebug, "Error: Can't read from PCI\n");
            return ~0U;
        }

        return ModsPciReadData.data;
    }
    else
    {
        MODS_PCI_READ   ModsPciReadData = {};

        // set private members
        ModsPciReadData.bus_number       = BusNumber;
        ModsPciReadData.device_number    = Device;
        ModsPciReadData.function_number  = Function;
        ModsPciReadData.address          = Register;
        ModsPciReadData.data_size        = 4;

        if (0 != ioctl(s_KrnFd, MODS_ESC_PCI_READ, &ModsPciReadData))
        {
            Printf(Tee::PriDebug, "Error: Can't read from PCI\n");
            return ~0U;
        }

        return ModsPciReadData.data;
    }
}

//------------------------------------------------------------------------------
// MapPhysicalMemory
//------------------------------------------------------------------------------

bool MapPhysicalMemory
(
    UINT32 PhysicalBase,
    size_t Size,
    UINT32** ppVirt
)
{
    void *pVirt = mmap64(0, Size, PROT_READ | PROT_WRITE,
                         MAP_SHARED, s_KrnFd, PhysicalBase);
    if (pVirt == MAP_FAILED)
        return false;
    *ppVirt = static_cast<UINT32*>(pVirt);
    return true;
}

static bool CheckDriverVersion(UINT32 Version)
{
    return true;
}

static bool GetAPIVersion(UINT32 &Version)
{
    MODS_GET_VERSION ModsGetVersion;

    if (0 != ioctl(s_KrnFd, MODS_ESC_GET_API_VERSION, &ModsGetVersion))
    {
        Printf(Tee::PriHigh, "Error: Can't get MODS kernel module API version\n");
        return false;
    }

    Version = ModsGetVersion.version;

    return true;
}

// -----------------------------------------------------------------------------
// Check if kernel module is loaded and initialize communication between
// user mode MODS and kernel side
bool OpenDriver()
{
    // Check if the MODS device exists
    struct stat buf;
    if (0 != stat(s_DeviceFileName, &buf))
    {
        // Driver is not present in the system, we have to run install script
        Printf(Tee::PriHigh, "Error: %s device not found.\n", s_DeviceFileName);
        Printf(Tee::PriHigh, "\n"
                "Please run the `install_module.sh --install' script as root\n"
                "to install the MODS kernel module.\n\n");
        return false;
    }

    // Check access permissions
    if (0 != access(s_DeviceFileName, R_OK | W_OK))
    {
        Printf(Tee::PriHigh, "Error: Unable to open %s. Please check your access permissions.\n", s_DeviceFileName);
        return false;
    }

    // Open the driver
    s_KrnFd = open(s_DeviceFileName, O_RDWR);
    if (s_KrnFd == -1)
    {
        Printf(Tee::PriHigh, "Error: Unable to open %s. Another instance of MODS may be running.\n", s_DeviceFileName);
        return false;
    }

    UINT32 version;
    if (!GetAPIVersion(version))
        return false;
    if (!CheckDriverVersion(version))
        return false;
    Printf(Tee::PriLow, "Current kernel module version is 0x%x\n", version);

    const UINT32 minDomainApiVerion = 0x353;
    if (version >= minDomainApiVerion)
        s_DomainApiSupport = true;

    return true;
}

void CloseDriver()
{
    if ((s_KrnFd != -1) && (close(s_KrnFd) == -1))
    {
        Printf(Tee::PriHigh, "Error: Unable to close %s\n", s_DeviceFileName);
    }
}
