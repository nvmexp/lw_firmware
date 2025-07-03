/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Mac MODS kernel extension driver

#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOLib.h>
#include <architecture/i386/pio.h>
#include "ModsDriver.h"
#include "LwModsUserClient.h"
#include "LwModsLog.h"

#define super IOService

// Do not user "super" here
OSDefineMetaClassAndStructors(com_lwidia_driver_LwModsDriver, IOService);

bool LwModsDriver::start(IOService *provider)
{
    bool success;

    LOG_ENT();
    success = super::start(provider);
    if (success)
    {
        registerService();
    }
    LOG_EXT();

    return success;
}
