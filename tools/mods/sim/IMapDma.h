/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// DO NOT EDIT
// See https://wiki.lwpu.com/engwiki/index.php/MODS/sim_linkage#How_to_change_ifspec

#ifndef _IMAP_DMA_H_
#define _IMAP_DMA_H_

#include "ITypes.h"
#include "IIface.h"

// The IMapDma interface provides methods for DMA mapping memory to devices.

class IMapDma : public IIfaceObject
{
public:
    // IIfaceObject Interface
    virtual void AddRef() = 0;
    virtual void Release() = 0;
    virtual IIfaceObject* QueryIface(IID_TYPE id) = 0;

    // IMapDma Interface

    // Set DMA mask (number of bits) for PCI device
    virtual int SetDmaMask(LwPciDev pciDev, LwU032 bits) = 0;

    // DMA map memory to a PCI device
    virtual int DmaMapMemory(LwPciDev pciDev, LwU064 physAddr) = 0;

    // Unmap DMA-maped memory from a PCI device
    virtual int DmaUnmapMemory(LwPciDev pciDev, LwU064 physAddr) = 0;

    // Get I/O virtual address for a PCI device (requires DmaMapMemory first)
    virtual int GetDmaMappedAddress(LwPciDev pciDev, LwU064 physAddr, LwU064* pIoVirtAddr) = 0;
};

#endif // _IMAP_DMA_H_
