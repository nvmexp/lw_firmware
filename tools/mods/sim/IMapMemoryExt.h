/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008,2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// DO NOT EDIT
// See https://wiki.lwpu.com/engwiki/index.php/MODS/sim_linkage#How_to_change_ifspec

#ifndef _IMAP_MEMORYEXT_H_
#define _IMAP_MEMORYEXT_H_

#include "ITypes.h"
#include "IIface.h"

// The IMapMemoryExt interface provides system memory map/remap/unmap functions
// which support UC/WC/Normal memory types
class IMapMemoryExt : public IIfaceObject
{
public:
    // IIfaceObject Interface
    virtual void AddRef() = 0;
    virtual void Release() = 0;
    virtual IIfaceObject* QueryIface(IID_TYPE id) = 0;

    //! IMapMemoryExt interface
    virtual int   MapMemoryRegion(
        void ** pReturnedVirtualAddress,
        LwU064 PhysicalAddress,
        size_t NumBytes,
        int Attrib,
        int Protect) = 0;
    virtual void  UnMapMemoryRegion(
        void * VirtualAddress) = 0;
    virtual int   RemapPages(
        void * VirtualAddress,
        LwU064 PhysicalAddress,
        size_t NumBytes,
        int Protect) = 0;
};

#endif // _IMAP_MEMORYEXT_H_
