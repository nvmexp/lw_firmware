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

#ifndef _MODSDRIVER_H_
#define _MODSDRIVER_H_

#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <sys/queue.h>

#define LwModsDriver com_lwidia_driver_LwModsDriver

class LwModsDriver : public IOService
{
    OSDeclareDefaultStructors(com_lwidia_driver_LwModsDriver);

protected:

public:
    virtual bool start(IOService *provider);
};

#endif /* _MODSDRIVER_H_ */

